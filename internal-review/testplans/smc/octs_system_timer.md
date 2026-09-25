# SMC `octs_system_timer` test audit and remediation plan

## Scope and audit basis

Static audit only; no implementation or test changes were made.

Audited files:

- `smc/peripherals/octs_system_timer/include/octs_system_timer.h`
- `smc/peripherals/octs_system_timer/src/octs_system_timer.cpp`
- `smc/peripherals/octs_system_timer/test/octs_system_timer_tb.cpp`
- `smc/peripherals/octs_system_timer/test/CMakeLists.txt`
- `smc/peripherals/octs_system_timer/CMakeLists.txt`
- `smc/peripherals/octs_system_timer/run_tests.sh`
- `smc/peripherals/octs_system_timer/doc/test_plan.adoc`
- `smc/scripts/enforce_line_coverage.sh`

## Behavior inventory

- Cycle-driven 64-bit timer with PRIMARY and SECONDARY modes selected by a live input.
- Primary START singlepulse loads a split 64-bit preset, enables counting, and starts a programmable-width sync pulse.
- Primary emits periodic credit pulses based on 8-bit `CREDIT_VAL`.
- Secondary synchronizes incoming sync/credit levels through a modeled four-cycle path and detects rising edges.
- Secondary counts by programmable STEP until its credit accumulator reaches/exceeds budget, then halts and counts starvation cycles.
- Credit pulse re-anchors to `expected_count + CREDIT_VAL`; sync pulse reloads preset.
- `CREDIT_EXPIRED` exposes a peak starvation count and clears on any write.
- GPIO enable drives an output; count, pulse, and debug signals are updated by a sole output method.
- MMIO aperture is 0x24 bytes with 32-bit aligned accesses; delay is mutable via CCI.
- Reset is asynchronous on assertion and synchronously holds state while low on clock edges.

## Existing test classification matrix

| Area | Existing coverage | Classification |
|---|---|---|
| Reset/registers | All named reset registers, representative masks/RO, GPIO | Semantic, socket-driven |
| Primary timing | Preset load, count, sync width, credit period/width | Strong cycle-semantic |
| Primary-secondary | End-to-end constant lag across periods | Strong integration at signal level |
| Secondary | Sync latency, STEP, starvation, replenish, re-anchor | Strong semantic |
| Credit expired | Growth and write-clear | Semantic; overflow/race gaps |
| TLM negative | Null, zero, width, alignment, aperture, BE, SW, command | Good baseline |
| CCI/delay | Exact preset delay and live mutation | Strong |
| Debug | Direct internal getters and dump-state branch | Mixed; some direct-state oracle |
| Constraint warning | One `CV<=PW` case and report-count check | Semantic but incomplete illegal-value matrix |
| Build/coverage | One comprehensive bench, source gate | Aggregate source coverage only |

## Evidence findings

### High

1. **Canonical SMC AXI sideband is absent from the target model and tests.**  
   Neither header nor source includes/extracts `smc_axi_extension`; `b_transport` at `octs_system_timer.cpp:384-438` ignores all extensions. The repository contract applies to SMC peripherals. Add canonical extension handling/inspection and attached/absent tests, even if authorization remains upstream.

2. **`CREDIT_VAL=0` causes unsigned-underflow behavior that is warned about but not semantically defined.**  
   Primary generation compares against `uint8_t(cv - 1)` at `octs_system_timer.cpp:137-139`, and the counter reset/wrap path does the same at `:164-171`. With `cv=0`, the expression becomes 255, yielding a 256-cycle behavior rather than a clean disabled/error state. The test only uses CV=4 with PW=8 (`octs_system_timer_tb.cpp:719-735`). Add CV=0 tests and either enforce/fatal or specify behavior.

3. **UBSan is not enabled.**  
   `CMakeLists.txt:27-31` enables only AddressSanitizer. Cycle arithmetic and narrowing make undefined-behavior coverage especially relevant.

4. **Reset during active protocol activity is not tested.**  
   `reset_state` clears pulse FSM, synchronizers, count, and credit state (`octs_system_timer.cpp:76-96`), but tests apply reset only between scenarios. Missing reset during sync pulse, credit pulse, synchronizer pipeline, starvation, and simultaneous MMIO START.

### Medium

5. **The synchronizer-latency oracle is partly tautological.**  
   The test compares lag against `octs_system_timer::SYNC_LATENCY_CYCLES` (`octs_system_timer_tb.cpp:560-569`), the same implementation constant that documents the model. If both implementation and constant drift together, the test passes. Derive the expected four cycles independently from the RTL/spec in the test.

6. **Direct debug state is used where architectural/signal checks should dominate.**  
   Tests call `dbg_expected_count`, `dbg_credit_expired`, and `dbg_cur_credits` at `octs_system_timer_tb.cpp:613-616,630-633,660-663,701-710`. These are useful white-box checks but do not prove MMIO/output behavior. Pair each with count/status/credit-expired/debug-output assertions.

7. **Input pulse shape and edge-detector behavior are incomplete.**  
   Tests use two-cycle pulses only. Missing one-cycle, shorter-than-clock/delta, long-held-high, back-to-back with one low cycle, simultaneous sync+credit, credit before sync, and pulses in PRIMARY mode. These directly exercise the four-stage synchronization and edge detection at `octs_system_timer.cpp:221-253`.

8. **Priority among simultaneous state transitions is unverified.**  
   Counter logic prioritizes idle/reset, then primary START; secondary sync before credit before ordinary step (`octs_system_timer.cpp:145-161`). Pulse FSM prioritizes START before credit (`:204-219`). Tests never drive simultaneous START/credit/sync or mode changes at a clock edge.

9. **Live mode switching is untested.**  
   `is_primary_i` is a run-time signal, affects both datapath and output gating (`octs_system_timer.cpp:121-139,164-202,265-274`), and does not reset sticky enable/pulse state. Switching PRIMARY↔SECONDARY while running/pulsing/starved can expose stale state.

10. **Programming edge values are incomplete.**  
    Missing PULSE_WIDTH=0 (effective 1), 1, 255; CREDIT_VAL=1/255; STEP=0/1/255; STEP greater than CV; CV=PW+1; and all max combinations. Only defaults, STEP=3, and one invalid CV/PW pair are covered.

11. **Counter and starvation overflow are untested.**  
    `timer_count_`, `expected_count_`, `credit_expired_`, and peak all wrap naturally (`octs_system_timer.cpp:150-160,180-235`). Add preset near `UINT64_MAX`, re-anchor overflow, and starvation near `UINT32_MAX` through a controlled back door or reduced-width test specialization if available.

12. **Split 64-bit reads have no coherency test.**  
    LO and HI are independently sampled (`octs_system_timer.cpp:305-310`). Reading across a rollover can produce a torn value; no latch/retry policy is documented or tested.

13. **START retrigger behavior is incomplete.**  
    A START written while already enabled reloads the preset in primary mode and may be ignored by the pulse FSM if a pulse is active (`octs_system_timer.cpp:145-153,206-219`). Test repeated START while idle, sync pulse active, credit pulse active, and in SECONDARY mode.

14. **TLM DMI/debug and several payload boundaries are missing.**  
    There is no `transport_dbg`, no DMI callback, and no test of stale `dmi_allowed`, BE pointer with zero BE length, streaming width greater than length, 64-bit address overflow, or initial non-zero delay. Existing TLM negatives are at `octs_system_timer_tb.cpp:774-823`.

15. **Constraint warning is “warn once” but warning lifetime/reset semantics are not tested.**  
    `ctrl_warned_` is not cleared by `reset_state` (`octs_system_timer.cpp:76-96,364-377`). After reset, a new invalid program does not warn. Confirm whether “once per model lifetime” is intended.

### Low

16. **Coverage-oriented dump-state setup has weak semantic value.**  
    `test_tlm_error_paths` ends by forcing state and calling `dump_state` specifically for a branch (`octs_system_timer_tb.cpp:774,826-834`) without asserting output text.

17. **CTest failure matching is narrow.**  
    `test/CMakeLists.txt:9-13` only treats `FAIL [line` as failure because one warning is expected. Process exit still protects normal runs, but precise expected-report handling is safer than regex accommodation.

18. **Coverage is aggregate source-only.**  
    `run_tests.sh:251-254` reports model/test; the shared gate checks aggregate `src/*.cpp`, not inline header logic or per-file touched coverage.

## Missing scenario inventory

### TLM2 and sideband

- Canonical AXI extension attached/absent and all sideband fields.
- Incoming delay accumulation from non-zero value; delay behavior on every error.
- BE pointer with zero and full BE lengths; SW 0/<len/=len/>len; stale DMI and response.
- Last valid register, all offsets, 0x24 boundary, `UINT64_MAX` address; debug/DMI policy.

### Registers and boundaries

- Every CTRL field at 0/1/max and cross-product cases.
- Reserved writes to every RO register while active, not just reset.
- START writes of 0, 1, all ones; repeated START and write-before-clock behavior.
- PRESET/count values around 32-bit and 64-bit rollover; LO/HI coherent-read strategy.
- CREDIT_EXPIRED clear while currently starved and on same cycle the peak updates.

### Timing, thread/event, reset

- One-cycle and long sync/credit levels through exact four-edge latency.
- Simultaneous sync+credit, repeated edges, credit-before-sync, ignored primary inputs.
- Reset at each synchronizer stage, each pulse-FSM state, starvation, and re-anchor.
- Live mode switch at idle/running/pulse/starved states.
- Clock periods other than 10 ns to prove behavior is cycle-based, not absolute-time based.

### Outputs and interrupt-equivalent signals

- Every output immediately after elaboration/reset, on mode switch, during both pulse states, and after reset.
- Debug outputs checked against architectural behavior at budget equality/overshoot.
- No IRQ exists; explicitly document that CREDIT_EXPIRED is polled and no interrupt scenario applies.

## Proposed test matrix

| ID | Layer | Scenario | Oracle |
|---|---|---|---|
| OCTS-TLM-01 | MMIO | Full malformed payload/boundary matrix | Exact response/delay/no mutation |
| OCTS-TLM-02 | MMIO | Canonical AXI extension and DMI/debug policy | Field preservation and denial |
| OCTS-REG-01 | MMIO | Exhaustive CTRL/START/GPIO/RO contracts | Independent RDL expectations |
| OCTS-PRI-01 | Clock | CV/PW/STEP boundary matrix | Exact count and pulse traces |
| OCTS-PRI-02 | Clock | Repeated START in each pulse state | Reload and pulse-FSM policy |
| OCTS-SEC-01 | Clock | Pulse width/spacing/edge-detect matrix | Independently counted latency |
| OCTS-SEC-02 | Clock | Simultaneous sync/credit and credit-before-sync | Spec priority and count |
| OCTS-MODE-01 | Clock | Live PRIMARY↔SECONDARY transitions | No stale/illegal outputs |
| OCTS-OVF-01 | Clock | 64-bit count and 32-bit expiry overflow | Defined wrap/saturation policy |
| OCTS-COH-01 | MMIO/clock | LO/HI read across rollover | Documented coherent-read result |
| OCTS-RST-01 | Clock | Reset in every pulse/sync/starvation phase | Full state/output cancellation |
| OCTS-TIME-01 | CCI/MMIO | Exact delay from non-zero input before/after mutation | Exact annotation |
| OCTS-BUILD-01 | Sanitizer | Test-only UB canary | UBSan fails the run |

## Verdict

**Best semantic suite of the six audited IPs, but still incomplete at edge conditions.** Primary/secondary cycle behavior is tested meaningfully end to end. Remaining high-risk gaps are missing canonical AXI sideband coverage, undefined `CREDIT_VAL=0` behavior, absent UBSan, and no reset/protocol concurrency. The direct debug and self-referential latency oracles should be supplemented with independent architectural checks.
