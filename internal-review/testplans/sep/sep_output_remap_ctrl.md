# SEP `sep_output_remap_ctrl` test audit

## Scope and audited files

- Model: `include/sep_output_remap_ctrl{,_base,_register}.h`, `src/sep_output_remap_ctrl{,_base}.cpp`
- Tests: `test/inc/sep_output_remap_ctrl_{base,}test.h`, `test/src/sep_output_remap_ctrl_basetest.cpp`, `test/src/sep_output_remap_ctrl_testbench.cpp`
- Build/test: `CMakeLists.txt`, `run_tests.sh`, `doc/test_plan.adoc`

## Behavior inventory

- Sixteen 64-bit `REGION_ATTRS` slots; bits `[55:0]` are RW and reset to zero.
- AP/STEE constructors select distinct window bases; parametric constructor validates a nonzero power-of-two region count no greater than 16.
- Data path computes the region index from adjusted address bits, combines programmed upper bits with the original lower 19 bits, forwards via `remapped_socket`, and restores the caller's address.
- A `sep_axi_extension`, when present, is retagged to `OTHERS_SOURCE_ID` downstream and restored upstream.
- `transport_dbg` applies the same remap/retag behavior.
- Active-low reset clears all region attributes.

## Existing test classification matrix

| Existing case | Behavior | Path | Classification |
|---|---|---|---|
| T1/T10 | construction reset and real reset pulse | CSR frontdoor + pin | Positive/reset |
| T2–T4 | region RW and 56-bit mask | CSR `b_transport` | Positive/boundary |
| T3 | all 16 slots | CSR loop with distinct expectations | Positive, not a blanket offset loop |
| T5/T6 | AP read/write remap | data `b_transport` | Positive |
| T7 | caller address restoration | data `b_transport` | Positive/protocol |
| T8 | documents ignored byte enables | CSR `b_transport` | Known limitation acceptance |
| T9 | debug remap | `transport_dbg` | Positive |
| T11–T13 | extension retag/restore/no-extension | both data paths | Positive |
| B1–B3 | STEE base and instance isolation | CSR/data | Positive |

## Coverage-shortcut findings

1. **High — the byte-enable test ratifies incorrect behavior.** The model itself documents that partial byte enables are not honored (`sep_output_remap_ctrl_register.h:13-24`), and T8 explicitly passes when disabled bytes are overwritten (`testbench.cpp:402-431`). This is coverage of a limitation, not verification of hardware-compatible strobes.
2. **High — no negative TLM protocol testing.** CSR helpers silently turn read errors into zero (`sep_output_remap_ctrl_basetest.cpp:30-33,67-70`), write helpers do not inspect response status (`:36-50,73-87`), and only two data helpers assert OK (`testbench.cpp:169-196`). Invalid offsets, lengths, streaming widths, null pointers, and byte-enable geometry are absent.
3. **Medium — constructor fatal branches are coverage-driven but untested.** Power-of-two and maximum-region checks at `sep_output_remap_ctrl.cpp:52-57` have no death/elaboration tests.
4. **Medium — only two remap indices are functionally sampled.** T5 uses region 0 and T6 region 3 (`testbench.cpp:340-380`); T3 proves CSR slot storage but not that data-path index extraction selects all slots or boundary addresses.
5. **Medium — downstream response propagation is not checked.** `data_b_transport` forwards and restores metadata (`sep_output_remap_ctrl.cpp:145-158`), but the stub always returns OK and zero delay (`testbench.cpp:74-79`), so errors and annotated delays are untested.
6. **Medium — coverage gate is aggregate over all `src/*` and `include/*`.** `CMakeLists.txt:180-209` extracts the full IP and the shared gate checks aggregate line percentage, which can hide a weak file behind highly exercised register templates.
7. **Medium — standalone ASan is not a clean-log gate.** `run_tests.sh:29-71` selects ASAN but sets no `ASAN_OPTIONS`, does not check logs, and permits `--asan --coverage` with last-option-wins instead of rejecting the conflicting request.

No private-public macro, direct callback invocation, report suppression, tautological expected-value calculation from DUT output, or test-only model branch was found.

## Missing scenarios

- First/last byte within every region and exact transitions between regions 0/1 and 14/15.
- Addresses below `REGION_BASE`, exactly at window end, and above the nominal window; current unsigned subtraction/wrap behavior is unspecified.
- Parametric `NUM_REGIONS` values 1, 2, 4, 8 and invalid 0, 3, 17; `IDX_START` 0 and dangerous shift limits.
- Maximum 56-bit offset, lower-bit contamination in programmed offsets, and remapped-address overflow.
- Downstream generic/address/command errors, delay accumulation, DMI flag, byte enables, streaming width, and arbitrary payload extensions.
- Debug return count/error and address/extension restoration when the downstream target fails.
- Reset asserted while traffic is forwarded and reset reassertion/deassertion semantics.
- Misaligned, partial, zero-length, cross-register, and out-of-range CSR transactions.

## Proposed testcases

| ID | Stimulus | Expected result | Path | TLM feature |
|---|---|---|---|---|
| ORC-001 | Program unique values in all 16 slots; access first/last address of each | Correct slot selected and low 19 bits preserved | CSR + data frontdoor | address boundaries |
| ORC-002 | Access exact region transitions | No off-by-one in index extraction | data `b_transport` | address |
| ORC-003 | Address below base and beyond 8 MiB window | Explicit documented wrap/reject policy | data `b_transport` | address error |
| ORC-004 | Downstream returns error and adds delay | Status/delay propagate; address and extension still restore | data `b_transport` | response/delay |
| ORC-005 | Repeat ORC-004 through debug | Return count/error policy and restoration hold | `transport_dbg` | debug |
| ORC-006 | Payload with extra custom extension plus SEP extension | Unknown extension preserved; SEP source ID retagged only downstream | both | extensions |
| ORC-007 | Valid partial CSR writes for each byte lane | Untouched bytes preserved, or explicit BYTE_ENABLE_ERROR | CSR `b_transport` | byte enables |
| ORC-008 | CSR lengths 0/1/4/8/9, misalignment, bad offset | Deterministic response; no adjacent corruption | CSR `b_transport` | length/streaming |
| ORC-009 | Parametric 1/2/4/8-region instances | Correct masks/indexing | data frontdoor | configuration boundary |
| ORC-010 | Invalid constructor configurations | Fatal report with expected ID/message | elaboration | negative config |
| ORC-011 | Reset during/around accesses | All slots clear only on assertion; routing remains deterministic | pin + frontdoor | reset/event |

## Verdict

**Partial pass.** Core remap, metadata restoration, reset, debug, and dual-instance behavior are tested through public sockets. The suite still accepts broken byte-enable semantics and lacks negative protocol, downstream-error/delay, complete region-boundary, and parametric-constructor coverage.
