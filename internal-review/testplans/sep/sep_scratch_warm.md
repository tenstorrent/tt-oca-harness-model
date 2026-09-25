# SEP `sep_scratch_warm` test audit

## Scope and audited files

- Model: `include/sep_scratch_warm{,_base,_register}.h`, `src/sep_scratch_warm{,_base}.cpp`
- Tests: `test/inc/sep_scratch_warm_{base,}test.h`, `test/src/sep_scratch_warm_basetest.cpp`, `sep_scratch_warm_test.cpp`, `sep_scratch_warm_testbench.cpp`
- Build/test: `CMakeLists.txt`, `run_tests.sh`, `doc/test_plan.adoc`

## Behavior inventory

- Eight 64-bit scratch slots, low 32 bits RW and high 32 bits reserved/read-zero.
- Byte-enable-aware write callbacks merge enabled bytes with current storage before applying the register mask.
- Active-low reset (sensitive to any edge, action only while low) clears all eight slots.
- Constructor clears storage.
- No interrupt, thread, timing annotation, or functional side effect exists.

## Existing test classification matrix

| Existing case | Behavior | Path | Classification |
|---|---|---|---|
| 001 | all eight reset values | frontdoor | Positive/reset |
| 002a | distinct per-entry 64-bit RW | frontdoor | Positive/alias |
| 002b | reserved high half on wide write | frontdoor | Mask |
| 002c | two high-byte writes preserve low half | frontdoor | Partial write |
| 002d | low-byte read/write | frontdoor | Partial write |
| 003 | programmed values then real reset pulse | frontdoor + pin | Reset/event |

## Coverage-shortcut findings

1. **High — TLM responses are never checked.** The 8-bit and 64-bit helpers initialize INCOMPLETE status but ignore it (`sep_scratch_warm_test.cpp:12-44`, `testbench.cpp:79-113`). A failed transaction that leaves zero/stale data can satisfy several checks.
2. **Medium — byte-enable callback coverage is narrow.** Tests use null byte-enable pointers and rely on transfer address/length to synthesize enables (`testbench.cpp:147-166`). Explicit enable arrays, disabled lanes, repeating patterns, and noncontiguous patterns never test `byte_enable_to_mask`/`merge_be` (`sep_scratch_warm.cpp:12-27`).
3. **Medium — no negative/boundary protocol cases.** Only aligned offsets and lengths 1 or 8 are used. There is no zero length, crossing an 8-byte word, end-of-aperture access, malformed streaming width, null pointer, or bad command.
4. **Medium — reset sensitivity is under-sampled.** A single low/high pulse with 1 ns waits (`testbench.cpp:187-196`) does not test initially-low reset, repeated low levels, write-while-reset, or same-delta ordering.
5. **Medium — build quality gates are incomplete.** Coverage aggregates `src/*` and `include/*` (`CMakeLists.txt:180-209`); standalone `--asan` does not configure/check sanitizer logs or leaks and option parsing allows ASAN/coverage last-option-wins (`run_tests.sh:26-66`).
6. **Low — dead test metadata adds no assurance.** `reg_map` in `sep_scratch_warm_basetest.cpp:5-6` is not consumed by the active suite.

No direct private/internal method invocation, private-public macro, blanket offset loop, tautological expected value, report suppression, or test-only model branch was found.

## Missing scenarios

- Every single-byte lane in low and reserved halves.
- Explicit byte-enable masks: `0x00`, each one-hot lane, aligned and unaligned halfwords, alternating/noncontiguous, and repeating masks shorter than data length.
- Partial write preservation across all slots and across a word boundary.
- Lengths 0, 2, 4, 7, 8, 9; unaligned offsets; exact aperture last byte and first invalid byte.
- Streaming widths 0, less than length, and greater than length.
- Null data pointer, IGNORE/unsupported command, response status.
- `transport_dbg` and DMI behavior of the register-memory target.
- Reset held low while writes arrive; initial low; same-delta assert/write/deassert.
- Concurrent accesses/reset event ordering.

## Proposed testcases

| ID | Stimulus | Expected result | Path | TLM feature |
|---|---|---|---|---|
| WARM-001 | Write/read distinct values in all slots and verify response | Exact values, OK status, no aliasing | `b_transport` | baseline |
| WARM-002 | One-byte access to each of 8 byte lanes | Low four lanes update independently; upper lanes remain zero | `b_transport` | length/address |
| WARM-003 | Explicit BE one-hot/aligned/noncontiguous/all-disabled | Enabled bytes merge; disabled bytes preserved; invalid patterns follow defined policy | `b_transport` | byte enables |
| WARM-004 | Cross-word and end-of-aperture transfers | No adjacent corruption; deterministic address error where invalid | `b_transport` | boundary |
| WARM-005 | Length/streaming/null/unsupported-command matrix | Correct error status and no mutation | `b_transport` | protocol negative |
| WARM-006 | Debug read/write of low/reserved halves | Correct count/data and mask behavior, no timing | `transport_dbg` | debug |
| WARM-007 | DMI request if supported by memory target | Explicit grant/refusal and permissions/range | DMI | DMI |
| WARM-008 | Hold reset low and attempt writes | Defined reset priority; final contents zero | pin + frontdoor | reset |
| WARM-009 | Assert reset in same delta as a write | Deterministic event ordering; no stale value | pin + frontdoor | delta timing |

## Verdict

**Partial pass.** The small functional surface is tested through the public socket and reset pin, including useful reserved-bit and narrow-access checks. Meaningful TLM protocol/error, explicit byte-enable, debug/DMI, boundary, and reset-ordering coverage is still absent, and helpers do not validate transaction responses.
