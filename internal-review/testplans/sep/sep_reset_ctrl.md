# SEP `sep_reset_ctrl` test audit

## Scope and audited files

- Model: `include/sep_reset_ctrl{,_base,_register}.h`, `src/sep_reset_ctrl{,_base}.cpp`
- Tests: `test/inc/sep_reset_ctrl_{base,}test.h`, `test/src/sep_reset_ctrl_basetest.cpp`, `sep_reset_ctrl_test.cpp`, `sep_reset_ctrl_testbench.cpp`
- Build/test: `CMakeLists.txt`, `run_tests.sh`, `doc/test_plan.adoc`

## Behavior inventory

- One 64-bit CSR, `SW_RESET_N`, with writable/readable bits `[6:0]` and reset value `0x7e`.
- A memory write callback stores the masked value, notifies `sw_reset_changed_`, and yields one delta only when invoked from an SC thread/cthread.
- A read callback returns the shadow value.
- Seven active-low reset outputs equal `global_rst_ni AND corresponding SW_RESET_N bit`.
- `update_rst_outputs` runs at time zero and on global reset/software-reset events.
- A low edge/level on global reset resets the CSR/shadow to `0x7e`.

## Existing test classification matrix

| Existing case | Behavior | Path | Classification |
|---|---|---|---|
| Reset values | CSR default `0x7e` | frontdoor `b_transport` | Positive/reset |
| Register access | patterns `0x55`, `0x2a` | frontdoor | Positive |
| Output updates | several masks and all seven one-hot writes | frontdoor/readback | Positive |
| Pin-level reset | `0x00`, `0x7f`, `0x7e` outputs | frontdoor + pins | Positive/event |
| Global override | assert/deassert global reset, read CSR/pins | pin + frontdoor | Positive/reset |

## Coverage-shortcut findings

1. **High — one advertised test does not inspect outputs.** `test_reset_output_updates` loops through values but only checks CSR readback (`sep_reset_ctrl_test.cpp:94-119`); despite its name it does not verify any reset output. Pin checks elsewhere cover only all-zero/all-one/default, not each one-hot mapping.
2. **High — `test_global_reset_behavior` is misclassified.** It only writes one-hot values and checks readback (`sep_reset_ctrl_test.cpp:122-136`); no global reset is driven there.
3. **Medium — expected constants are imported from the DUT.** `kMask` and `kReset` alias model constants (`sep_reset_ctrl_test.cpp:14-17`), reducing independence from a correlated specification error. The unused base-test map duplicates independent values but is never used by the active tests.
4. **Medium — debug callback branch is untested.** The model explicitly has a no-process/`transport_dbg` path that must avoid `wait()` (`sep_reset_ctrl.cpp:97-105`), but tests use only `b_transport`.
5. **Medium — no back-to-back same-delta assertion/deassertion test.** The callback contains special yielding logic specifically to prevent merged event notifications (`sep_reset_ctrl.cpp:90-104`), yet tests insert 1 ns waits (`testbench.cpp:109-160`), bypassing the race it was written to solve.
6. **Medium — TLM helpers cover only ideal full-width accesses.** They use length/stream width 8, no byte enables, and assert only OK (`sep_reset_ctrl_test.cpp:25-62`); invalid offsets, partial writes, and malformed payloads are absent.
7. **Medium — build quality gates are incomplete.** Coverage is aggregate over `src/*` and `include/*` (`CMakeLists.txt:185-212`), while standalone ASAN has no log/leak enforcement and option parsing does not reject combined ASAN/coverage (`run_tests.sh:29-71`).

No private-public macro, direct private callback call, blanket offset loop, report suppression, or test-only model branch was found.

## Missing scenarios

- Independent pin mapping for each of the seven bits, including transitions high→low and low→high.
- Reserved bits `[63:7]` written as ones, partial byte writes, and preservation of unaffected implemented bits.
- Byte enables, short/long length, unaligned address, cross-register transfer, zero length, bad offset, and streaming width mismatch.
- `transport_dbg` read/write, especially the no-process callback branch and immediate-vs-delta output timing.
- Two writes in one delta cycle (assert then release) and output observation at delta boundaries.
- Global reset asserted at time zero, held low while software writes, repeated low events, and software write immediately after release.
- Reset while a callback is active and deterministic event ordering.
- DMI/debug policy of the underlying register memory.

## Proposed testcases

| ID | Stimulus | Expected result | Path | TLM feature |
|---|---|---|---|---|
| RST-001 | Write each one-hot and inverse-one-hot value | Exactly the matching output changes; all others remain correct | frontdoor + pins | functional/event |
| RST-002 | Write `~0ULL` | CSR reads `0x7f`; reserved bits read zero | frontdoor | register mask |
| RST-003 | Byte/halfword writes with enables | Implemented bits merge correctly; disabled lanes unchanged | frontdoor | byte enables/length |
| RST-004 | Bad offset, unaligned, length 0/1/4/8/9 | Deterministic status and no output corruption | frontdoor | protocol negative |
| RST-005 | Assert and deassert software bits in consecutive zero-time writes | Low pulse is observable before release; no merged-event loss | frontdoor SC_THREAD | delta timing |
| RST-006 | Debug write from `sc_main`/method context | No illegal wait; defined output update timing and readback | `transport_dbg` | debug/process context |
| RST-007 | Hold `global_rst_ni=0`, attempt writes | Outputs stay low and reset policy for CSR is explicit | pin + frontdoor | reset priority |
| RST-008 | Start simulation with global reset initially low/high | Time-zero outputs always match documented state | elaboration/event | initialization |
| RST-009 | Disable/re-enable global reset around programmed value | CSR restores `0x7e`, pins restore default mapping | pin + frontdoor | reset |

## Verdict

**Partial pass.** Basic CSR, default, global-reset, and coarse pin behavior are frontdoor-tested. The suite misses the model's most timing-sensitive callback branch, one-hot output mapping, malformed TLM requests, partial accesses, and debug transport. Two named tests overstate what they assert.
