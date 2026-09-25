# SEP `sep_scratch_cold` test audit

## Scope and audited files

- Model: `include/sep_scratch_cold{,_base,_register}.h`, `include/sep_status_decoder.h`, `include/status_values.h`, `src/sep_scratch_cold{,_base}.cpp`
- Shared behavior: `common/include/virt_console_decoder.h`
- Tests: `test/inc/sep_scratch_cold_{base,}test.h`, `test/src/sep_scratch_cold_basetest.cpp`, `sep_scratch_cold_testbench.cpp`
- Build/test: `CMakeLists.txt`, `run_tests.sh`, `doc/test_plan.adoc`

## Behavior inventory

- Eight 64-bit slots with writable/readable low 32 bits and reserved upper 32 bits.
- Active-low cold-reset negedge clears all slots; no warm-reset input exists.
- `SCRATCH[2]` post-write callback decodes virtual-console ASCII/HEX16/DEC24 words.
- `SCRATCH[1]` post-write callback decodes firmware status words and resolves names from a build-configured TSV/C header.
- Three test-support handshake callbacks recognize magic values in slots 0, 4, and 6 and directly place acknowledgements in slots 1, 5, and 7.
- CCI parameters enable console/status decoding and set verbosity.

## Existing test classification matrix

| Existing case | Behavior | Path | Classification |
|---|---|---|---|
| 001/001b/010 | constructor/direct reset, real cold pulse, retention without pulse | mixed direct/frontdoor/pin | Reset |
| 002–004 | per-slot RW, independence, reserved mask | frontdoor | Positive/boundary |
| 005–008 | magic acknowledgements and one-off non-magic values | frontdoor | Mixed |
| 009 | CCI defaults | direct parameter read | Configuration |
| 010/012 | console opcode callbacks | frontdoor to slot 2 | Positive, output not asserted |
| 011 | status type/stage branches | frontdoor to slot 1 | Positive, output not asserted |
| 013 plus duplicate unit cases | pure decoder guards/parser branches | direct decoder calls | Unit/coverage |

## Coverage-shortcut findings

1. **High — multiple tests reset through a public internal helper.** Cases 001, 002–008, and decoder cases call `dut.reset_all_registers()` directly (`testbench.cpp:162,227,261,294,319,339,359,378,466,510,552`). This bypasses the reset port/event path and inflates coverage of storage behavior; 001b is the valid frontdoor reset test.
2. **High — console/status callback tests do not assert emitted text.** `test_virt_console_decode`, `test_virt_console_hex16`, and `test_status_decoder_types` only verify the final scratch value (`testbench.cpp:463-495,548-568,506-539`). Comments claim stdout effects, but malformed formatting, wrong labels, duplicate/missing emission, and parameter disable behavior can pass.
3. **High — production model contains testbench-emulation branches.** Magic-value callbacks directly mutate register memory (`sep_scratch_cold.cpp:80-115`). These are explicitly VP test acknowledgements, not hardware scratch semantics, and create test-only behavior in the model.
4. **Medium — direct internals are used inside model callbacks.** Callbacks read/write `memory.memory_block[...]` (`sep_scratch_cold.cpp:40-45,72-76,88-114`) rather than exercising register APIs. Tests validate results but not access-mask/event semantics of those backdoor writes.
5. **Medium — tests never check TLM response status.** Helpers initialize INCOMPLETE then ignore the result (`testbench.cpp:79-106`), so decode/protocol failures can be mistaken for data values.
6. **Medium — parameter tests read DUT parameters directly.** `test_cci_param_defaults` (`testbench.cpp:717-739`) verifies only defaults, not broker presets or runtime behavior; it also accepts any verbosity `>=1` rather than the specified default.
7. **Medium — duplicated decoder-only tests are coverage padding.** `test_decoder_isolation` and later `test_status_decoder_unit`/`test_virt_console_unit` substantially repeat direct pure-function branches (`testbench.cpp:570-710,741-906`) without increasing frontdoor callback assurance.
8. **Medium — build quality gates are aggregate/incomplete.** Coverage extracts all `src/*` and `include/*` (`CMakeLists.txt:208-235`), while standalone ASAN neither verifies logs/leaks nor rejects ASAN+coverage option combinations (`run_tests.sh:26-66`).

No private-public macro, blanket address sweep, tautological expected value, or report suppression was found.

## Missing scenarios

- Capture and assert exact console/status emitted lines through the real slot callbacks.
- CCI presets disabling console/status before construction; confirm no output while storage still updates.
- Partial writes to callback slots: byte/halfword/upper-half, byte enables, and callback firing count.
- Invalid offsets, misalignment, zero/short/long lengths, streaming width mismatch, null data, debug transport, and DMI policy.
- Reset while an ASCII line is buffered; decoder state is not reset by `cold_reset_handler`, which only clears registers.
- Cold reset held low with writes, repeated assertion, and assertion in the same delta as a callback.
- Status-name file absent/malformed/duplicate/overflow values through model construction.
- Magic value preceded/followed by partial writes, ack-slot prepopulation, and reset interactions.
- Confirm warm-reset retention in an integrated reset network rather than by mere passage of time.

## Proposed testcases

| ID | Stimulus | Expected result | Path | TLM feature |
|---|---|---|---|---|
| COLD-001 | Program all slots, pulse `cold_rst_ni` | All slots clear through the pin path | frontdoor + pin | reset/event |
| COLD-002 | Trigger each console opcode and capture stream | Exact one-line text, buffering, newline, and no duplicate emission | frontdoor slot 2 | callback/event |
| COLD-003 | Trigger all status labels, known/unknown names | Exact stage/severity/code/name line | frontdoor slot 1 | callback |
| COLD-004 | Construct with both decoder enables false | No emission; scratch readback remains correct | CCI preset + frontdoor | configuration |
| COLD-005 | Byte/halfword writes with repeating enables | Correct low-half merge; reserved bytes unchanged; callback policy explicit | frontdoor | byte enables/length |
| COLD-006 | Bad offset, unaligned/cross-slot, malformed width | Error response and no corruption | frontdoor | protocol negative |
| COLD-007 | `transport_dbg` to ordinary and callback slots | Defined side effects/count; no timing | debug | debug transport |
| COLD-008 | Reset with partial console line buffered | Decoder state reset or documented retention | pin + callback | reset/internal state |
| COLD-009 | Magic/non-magic boundary and prefilled ack slots | Ack only on exact full value; deterministic overwrite policy | frontdoor | side effect |
| COLD-010 | Integrated warm reset followed by cold reset | Warm retains; cold clears | platform/frontdoor | reset domains |
| COLD-011 | Missing/malformed name-table configuration | Deterministic warning and `SEP_MSG_UNKNOWN`, no crash | construction + frontdoor | negative config |

## Verdict

**Partial pass with a model-purity concern.** Storage and cold-reset behavior have meaningful frontdoor coverage, but direct reset calls and direct decoder unit calls inflate coverage, emitted behavior is largely unasserted, and the production model includes explicit test-handshake branches. TLM negative/partial/debug cases and reset of decoder state remain open.
