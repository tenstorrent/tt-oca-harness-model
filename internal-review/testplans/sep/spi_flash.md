# SEP `spi_flash` test audit

## Scope and audited files

- Model: `include/spi_flash.h`, `spi_flash_model.h`, `spi_flash_sfdp.h`, `src/spi_flash.cpp`, `spi_flash_model.cpp`
- Tests: `test/src/spi_flash_test.cpp`, `spi_flash_sc_test.cpp`, `spi_flash_sfdp_utils.cpp`, `test/inc/spi_flash_sfdp_utils.h`
- Build/test: `CMakeLists.txt`, `run_tests.sh`, `doc/test_plan.adoc`

## Behavior inventory

- Pure C++ NOR model with erased `0xff` storage, bit-clearing page program, 256-byte page wrap, 4/32/64 KiB and chip erase, WEL, two status registers, suspend/resume, 3/4-byte addressing, JEDEC ID, SFDP ROM, reset, and file/backdoor access.
- SystemC `spi_if` wrapper accumulates TX segments across CSAAT, parses opcode/address/data, streams memory reads across RX segments, serves control responses once, returns idle-high for bare RX, and clears protocol state on reset.
- No MMIO/TLM generic-payload socket exists; the relevant frontdoor is `spi_if`.
- Operations are intentionally untimed in the flash; host timing lives in `spi_controller`.

## Existing test classification matrix

| Existing group | Behavior | Path | Classification |
|---|---|---|---|
| A | SFDP DWORD 1–16 setters/getters/serialization | direct pure objects | Unit/boundary |
| B/C | SFDP ROM/table/parser utilities | direct pure objects/files | Unit/negative |
| D | command set, WEL, program/erase, SFDP, reset, 4-byte, backdoor | direct `process_command` | Mixed core |
| SC.1–SC.12 | WREN/status, 1/2-segment program, read, erase, SFDP, suspend, reset, 4-byte, bare RX, unknown opcode, RDID | `spi_if` export via port | Frontdoor mixed |

## Coverage-shortcut findings

1. **High — default and default ASan runs omit the SystemC wrapper test.** `run_tests.sh:72-74` executes only `spi_flash_test`; `doc/test_plan.adoc:11-16,23-29` confirms only `--ctest`/coverage run both. Therefore normal release and `--asan` can pass while `spi_flash.cpp` is broken.
2. **High — chip erase reports success when it did nothing.** `process_command` always returns true for CHIP_ERASE (`spi_flash_model.cpp:381-384`) although `handle_chip_erase` silently refuses missing WEL or suspended state (`:221-237`). The test explicitly accepts `ok == true` without WEL (`spi_flash_test.cpp:1417-1427`), converting a failed operation into a passing command result.
3. **High — most functional coverage bypasses the public SystemC path.** The large D suite calls `spi_flash_model::process_command`, `read_byte`, and `write_byte` directly. This is valid core-unit coverage, but it does not establish segment framing, CSAAT state, reset-event ordering, pointer contracts, or wrapper error recovery.
4. **Medium — helper segment calls often ignore intermediate return values.** Header-phase calls in `cmd_program_2seg`, `cmd_read`, status, ID, and 4-byte helpers are not asserted (`spi_flash_sc_test.cpp:145-159,209-223,231-242,250-259,500-545`). A failure in the first segment can be masked by the final call.
5. **Medium — malformed segment/header behavior is untested.** `parse_address` zero-fills missing address bytes (`spi_flash.cpp:133-144`), and unknown opcodes default to three address bytes (`:61-109`); tests send only complete legal headers or a one-byte unknown final command.
6. **Medium — null-pointer behavior is only partly exercised.** TX with null data silently accumulates nothing (`spi_flash.cpp:176-182`); RX with null data still advances `m_read_bytes` (`:205-225`). No test defines whether this is accepted or an error.
7. **Medium — reset mid-command is absent.** Reset clears accumulator/read progress (`spi_flash.cpp:48-55`), but tests pulse reset only between complete commands (`spi_flash_sc_test.cpp:440-479`).
8. **Medium — read streaming uses one segment only.** The wrapper specifically supports multiple chained RX segments (`spi_flash.cpp:201-227`), yet SC reads have one RX segment; no test validates advancing `m_read_bytes`, final cleanup, or error cleanup.
9. **Medium — direct backdoor access is heavily used to set preconditions.** Core erase/file tests seed memory with `write_byte` (`spi_flash_test.cpp:965-1014,1226-1251,1258-1287`). This is acceptable for setup but should be paired with frontdoor verification for each command family.
10. **Medium — standalone sanitizer control is incomplete.** `run_tests.sh:29-71` does not set/check ASAN/UBSAN logs or leaks and does not reject combined ASAN/coverage flags. Coverage does run both binaries through `CMakeLists.txt:239-255`.

No private-public macro, report suppression, blanket address loop used as the sole oracle, or compile-time test-only model branch was found.

## Missing scenarios

- Multi-RX-segment streaming read (including 1-byte chunks, mixed chunk lengths, final CSAAT release, and back-to-back commands).
- Reset assertion after header, after intermediate RX, and during a multi-segment program.
- Truncated opcode/address headers, zero-length segments, TX/BIDIR with null TX, RX/BIDIR with null RX, and inconsistent direction/pointers.
- CSID and speed changes while CSAAT is active; wrapper currently ignores both.
- Dummy segments and dummy bytes for fast/SFDP reads; current address parsing treats all accumulated post-address TX bytes as program data for non-reads and does not model read latency bytes.
- Read crossing flash end, program/erase partially or wholly out of bounds, page wrap at last flash page, and integer address wrap.
- Empty program payload, program larger than one page, maximum 512-byte controller segment.
- All erase opcodes without WEL and while suspended, with command return value checked.
- READ_STATUS/RDID response lengths 0, 1, 2, 3, >3 and repeated RX segments.
- File I/O permission failure, short file preserving untouched tail, oversized truncation, and parent-directory creation failure.
- SFDP zero-sized model/density edge and parameter-table modifications beyond density.
- Timing is intentionally absent; a test should explicitly assert the flash adds no wait and relies on host timing.

## Proposed testcases

| ID | Stimulus | Expected result | Path | Interface feature |
|---|---|---|---|---|
| SPIF-001 | Header + three chained RX segments with varied lengths | Contiguous data, exact running address, state cleared on final CS release | `spi_if` | CSAAT/streaming |
| SPIF-002 | Reset after header and after first RX chunk | Accumulator/read offset cleared; next command starts clean; memory preserved | `spi_if` + pin | reset/event |
| SPIF-003 | Truncated 3B/4B headers and zero-length segment | Explicit failure, no address-zero operation | `spi_if` | negative framing |
| SPIF-004 | Null TX/RX for each direction | Deterministic failure/no state advance; no crash | `spi_if` | pointer contract |
| SPIF-005 | WREN absent/suspended for every erase opcode | Return false and memory unchanged | core + `spi_if` | command/status |
| SPIF-006 | Program lengths 0,1,255,256,257,512 near page end | Correct empty policy and page wrap; no next-page corruption | `spi_if` | length boundary |
| SPIF-007 | Read/program/erase at last byte and beyond capacity | `0xff` reads; no integer wrap or unintended low-address writes | both | address boundary |
| SPIF-008 | Fast/dual/quad/SFDP with explicit dummy segments | Address unaffected; expected data returned | `spi_if` | dummy/direction/speed |
| SPIF-009 | Status/RDID RX lengths and repeated reads | Defined truncation/fill behavior; accumulator clears each command | `spi_if` | response length |
| SPIF-010 | CSID/speed changes during CSAAT | Explicit accept/reject policy; no cross-command state leak | `spi_if` | sideband/config |
| SPIF-011 | File short/oversize/unwritable paths | Correct return and deterministic retained bytes | backdoor API | file negative |
| SPIF-012 | Run both executables in release and ASan defaults | Wrapper and core always execute under sanitizers | build system | quality gate |

## Verdict

**Partial pass.** Core command and SFDP coverage is extensive, and the wrapper has genuine frontdoor tests. However, default/ASan execution omits the wrapper, chip erase falsely returns success on refused operations, and the most stateful wrapper behavior—multi-RX streaming, malformed framing, null pointers, and reset mid-command—is not tested.
