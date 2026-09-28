# SMC `plic` test audit and remediation plan

## Scope and audit basis

Static audit only; no implementation or tests were edited.

Audited files:

- `smc/peripherals/plic/include/plic.h`
- `smc/peripherals/plic/src/plic.cpp`
- `smc/peripherals/plic/test/plic_tb.cpp`
- `smc/peripherals/plic/test/CMakeLists.txt`
- `smc/peripherals/plic/CMakeLists.txt`
- `smc/peripherals/plic/run_tests.sh`
- `smc/peripherals/plic/doc/test_plan.adoc`
- `smc/scripts/enforce_line_coverage.sh`

## Behavior inventory

- Configurable 1–1023 interrupt sources and one or more contexts, resolved via immutable CCI parameters.
- Priority source registers, packed pending words, per-context packed enable banks, threshold and claim/complete context blocks.
- Priority/threshold are 3-bit; source zero is reserved; unused final enable bits are masked.
- Rising source edges latch pending unless the source is globally claim-in-flight.
- Arbitration selects enabled pending source with priority strictly above threshold; highest priority then lowest ID.
- Claim clears pending and marks source in flight. Complete clears in-flight and immediately re-pends if the input remains high.
- Context outputs update through a single event-driven method.
- Active-low reset clears all state.
- MMIO accepts 32-bit aligned accesses in a 4 MiB aperture and adds mutable CCI delay.
- `transport_dbg` peeks CLAIM without claiming; debug writes use normal write behavior.
- Canonical AXI extension is extracted but not enforced; DMI is denied on successful traffic.

## Existing test classification matrix

| Area | Existing coverage | Classification |
|---|---|---|
| Reset/CSRs | Reset, priority mask, source 0, pending RO, context hole | Semantic |
| Source flow | Rising edge, enable, threshold, claim, complete high/low | Semantic, mostly source 7 |
| Arbitration | Higher priority and one equal-priority tie | Semantic |
| Contexts | Context 0 and context 4 isolation | Semantic, sparse |
| Packed maps | One low-word source and final-word mask | Partial; boundary sources not driven |
| TLM negative | Width, alignment, aperture, bad command | Incomplete payload protocol |
| Debug | CLAIM peek, general debug R/W, all `dbg_*`, dump | Mixed; direct-state heavy |
| CCI | Presets, metadata, mutable handle | Parameter value only; delay not measured |
| Sideband/DMI | Model extracts extension/denies DMI | No tests |
| Build/coverage | One bench; aggregate source gate | No independent negative/timing bench |

## Evidence findings

### Critical

1. **A completion write for a source that was never claimed can manufacture a pending interrupt.**  
   `complete` clears `claim_in_flight_[src]` without checking that it was set, then re-latches pending whenever the line is high (`plic.cpp:340-353`). Firmware can write an arbitrary valid high source ID to CLAIM/COMPLETE and create pending state. No test covers spurious, duplicate, wrong-source, or wrong-context completion. Completion must be accepted only under the spec’s ownership/in-flight rules.

### High

2. **The TLM target can dereference a null data pointer and ignores byte enables/streaming width.**  
   `b_transport` validates only length/alignment/aperture (`plic.cpp:554-568`) before memcpy at `:576-585`. Current raw tests always provide a pointer and set null BE/equal streaming width (`plic_tb.cpp:110-124`). Add full generic-payload validation.

3. **A source held high through reset may never become pending after reset release.**  
   Reset clears `last_level_` (`plic.cpp:180-188`), but `src_method` is sensitive only to source changes (`:132-136`) and reset deassertion does not scan inputs. If a source is already high and remains high, no event occurs to latch it. Test and fix according to level-gateway semantics.

4. **UBSan is absent.**  
   `CMakeLists.txt:47-51` enables only AddressSanitizer.

5. **Canonical AXI sideband is fetched but never verified.**  
   `plic.cpp:570-574` discards the extension. No test attaches it or checks no-extension behavior. Upstream filtering may own authorization, but the endpoint contract still needs a field-integrity/platform test.

### Medium

6. **The test uses direct state/debug methods for core protocol assertions.**  
   Pending and arbitration are often asserted through `dbg_pending`/`dbg_claim_top` (`plic_tb.cpp:266-268,311-326,332-343`) instead of pending words, CLAIM reads, and `ctx_out`. This can hide decode and side-effect defects. Keep debug API conformance separate.

7. **The final-word mask oracle repeats implementation arithmetic.**  
   `plic_tb.cpp:483-494` computes `W`, `valid_bits`, and expected mask with essentially the same formula as `plic.cpp:485-499`. Use independent fixed cases such as source counts 31, 32, 33, 63, 64, 65, 1023 with literal expected masks.

8. **Packed-word boundaries and maximum source are not functionally tested.**  
   The CCI preset reduces the DUT to 64 sources (`plic_tb.cpp:608-618`), but active interrupt testing stays at IDs 3, 5, 7, 11, 13. Sources 31/32/33/63/64, pending word 2 boundary, and default 336/source 336 behavior are absent.

9. **Cross-context claim concurrency is incomplete.**  
   One pending source may be enabled in multiple contexts. Tests show both outputs can assert, but do not cover near-simultaneous CLAIM reads, claim in one context clearing the other, completion by wrong context, or two different sources claimed by different contexts.

10. **Duplicate/racing source edges are untested.**  
    Missing: pulse while disabled, multiple pulses before claim, deassert/reassert while in-flight, reassert in same delta as completion, source transition concurrent with reset/claim/complete, and held-high behavior across priority/enable changes.

11. **Access-delay mutability is not behaviorally tested.**  
    The CCI test mutates and restores the handle (`plic_tb.cpp:532-551`) but no transaction captures delay. Assert exact delay accumulation before and after mutation, including errors.

12. **DMI denial is only implemented on successful accesses.**  
    `set_dmi_allowed(false)` occurs at the end of successful/decode-handled traffic (`plic.cpp:593-596`). Early errors leave a stale incoming DMI-allowed flag untouched. There is no `get_direct_mem_ptr` test.

13. **`transport_dbg` writes have architectural side effects.**  
    Debug writes call `reg_write` (`plic.cpp:641-648`), so writing CLAIM/COMPLETE can complete/re-pend, and priority/enable writes affect IRQ. Documentation calls it “back-door ... no side effects” (`plic.h:373-382`), which is true only for CLAIM reads. Clarify and test the write policy.

14. **`transport_dbg` does not validate pointer, BE, or streaming fields.**  
    It checks only length/alignment/aperture (`plic.cpp:617-648`). Add malformed debug traffic and exact transferred-byte behavior.

15. **Address-map hole/boundary coverage is sparse.**  
    Missing first/last priority, pending words, context enable padding, context just beyond configured contexts but within 4 MiB, last context suboffsets, 0x3ffffc, and 0x400000. RAZ/WI versus fault policy should be explicit for each.

16. **Reset output initialization relies on an asserted reset.**  
    All processes use `dont_initialize` (`plic.cpp:132-146`). Tests assert reset before checking. Add time-zero/deasserted-reset contract and repeated reset while IRQ is high/in-flight.

### Low

17. **Broad report suppression is present.**  
    `plic_tb.cpp:593-596` globally suppresses `SC_ID_LOGIC_X_TO_BOOL_`. Scope it or remove the source of unknown-to-bool conversion so unrelated occurrences are visible.

18. **The model comments claim a fixed access-delay member that does not exist.**  
    Header comments at `plic.h:229,368` refer to `access_delay_`, while implementation re-reads CCI directly (`plic.cpp:593`). This is documentation drift, not a test defect, but timing tests should encode the actual contract.

19. **Coverage is aggregate source-only and sanitizer naming overstates the implementation.**  
    `run_tests.sh:259-262` reports model/test; the shared gate checks aggregate source. The checked-in test plan says “ASan + UBSan,” but CMake does not enable UBSan.

## Missing scenario inventory

### TLM2, sideband, DMI/debug

- Null data pointer, zero length, byte enables (including pointer+zero length), streaming width 0/<4/>4, stale response/DMI.
- Exact success delay from non-zero incoming annotation, live CCI mutation, and error-delay policy.
- Canonical AXI extension with all source/privilege/security/fetch/lock/ID/user fields and absent extension.
- `transport_dbg` malformed payloads and read/write side-effect policy; explicit DMI callback denial.

### Register map and boundaries

- Source counts 1, 31, 32, 33, 63, 64, 65, 336, 1023.
- Active sources at every word boundary and final source; literal expected pending/enable masks.
- Priority source zero/one/final/final+1; all priority and threshold values 0–7.
- Context counts 1 and 8+; first/final configured context, unconfigured context apertures, enable padding and context holes.

### Interrupt protocol, timing, reset

- Priority exactly equal threshold, priority zero, dynamic reprioritization while pending.
- Same source enabled in multiple contexts with simultaneous claims.
- Different sources claimed concurrently; tie across word boundaries.
- Wrong/duplicate/spurious/out-of-range/source-zero completion.
- Pulse while disabled; repeated pulse before claim; edge while in-flight; held-high across reset; reset during claim-in-flight.
- IRQ enable/priority/threshold updates in same delta as source edge/claim/complete.
- Time-zero output, reset assertion and release with low/high sources.

## Proposed test matrix

| ID | Layer | Scenario | Oracle |
|---|---|---|---|
| PLIC-TLM-01 | MMIO | Full malformed generic-payload matrix | Exact response, no state, delay/DMI policy |
| PLIC-TLM-02 | MMIO | Canonical AXI extension attached/absent | Field integrity and platform-filter behavior |
| PLIC-TLM-03 | Debug/DMI | CLAIM peek, debug writes, malformed debug, DMI | Explicit side-effect and byte-count contract |
| PLIC-MAP-01 | MMIO | Source-count boundary parameter sweep | Literal masks and no aliasing |
| PLIC-MAP-02 | MMIO | Context/padding/window boundaries | RAZ/WI versus address error |
| PLIC-ARB-01 | Signals/MMIO | Priorities/thresholds 0–7 and word-boundary ties | CLAIM and `ctx_out` |
| PLIC-CTX-01 | Signals/MMIO | One source visible to multiple contexts | Atomic global claim behavior |
| PLIC-CMP-01 | MMIO | Correct, wrong, duplicate, spurious completion | No manufactured pending; ownership policy |
| PLIC-SRC-01 | Signals | Pulse/held/re-edge matrix before/during in-flight | Pending words and IRQ |
| PLIC-RST-01 | Signals | High source across reset; reset while in-flight | Defined relatch and clean state |
| PLIC-TIME-01 | CCI/MMIO | Exact delay before/after mutation | Exact annotation |
| PLIC-BUILD-01 | Sanitizer | Test-only UB canary | UBSan fails |

## Verdict

**Core arbitration is covered, but protocol correctness is not yet sufficient.** The critical defect candidate is spurious completion manufacturing a pending interrupt. High-risk gaps also include null-pointer/BE/streaming handling, held-high source behavior across reset, absent UBSan, and no sideband proof. The suite’s concentration on low source IDs and direct debug oracles means packed-map and multi-context correctness remain under-tested despite high line coverage.
