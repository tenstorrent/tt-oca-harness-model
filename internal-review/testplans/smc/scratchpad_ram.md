# SMC `scratchpad_ram` test audit and remediation plan

## Audit scope

Audited:

- `smc/peripherals/scratchpad_ram/include/scratchpad_ram.h`
- `smc/peripherals/scratchpad_ram/src/scratchpad_ram.cpp`
- `smc/peripherals/scratchpad_ram/test/scratchpad_ram_tb.cpp`
- `smc/peripherals/scratchpad_ram/test/scratchpad_ram_neg_tb.cpp`
- `smc/peripherals/scratchpad_ram/test/fixtures/scratchpad_sanity.rv64.hex`
- `smc/peripherals/scratchpad_ram/test/CMakeLists.txt`
- `smc/peripherals/scratchpad_ram/CMakeLists.txt`
- `smc/peripherals/scratchpad_ram/run_tests.sh`

No model or test was modified.

## Model behavior inventory

- Ports: TLM read/write target and active-low reset input; reset intentionally retains SRAM.
- Memory: CCI-sized byte vector, native/ECC granule 8 bytes, accepted transfer sizes 1/2/4/8 with natural alignment.
- Frontdoor reads/writes: byte slices, repeating byte-enable masks, range/alignment/streaming checks, mutable annotated delay, DMI disallowed on successful transactions.
- Debug paths: `transport_dbg`, `dbg_read32/64`, `dbg_load_bytes`, ECC injection/clear, configuration accessors, and dump.
- ECC abstraction: sparse per-word correctable/uncorrectable flags; correctable read scrubs; uncorrectable read fails; any overlapping write clears error state.
- Preload: hex and binary loaders, auto suffix selection, comments/prefix handling, zero-fill, zero-only overflow tolerance, and fatal errors for malformed/missing/empty inputs.
- CCI: immutable size/file/format/ECC and mutable access delay.

## Existing test classification

| Existing test area | Classification | Rationale |
|---|---|---|
| Frontdoor 1/2/4/8-byte read/write and reset retention (`scratchpad_ram_tb.cpp:220-277`) | Genuine | Public TLM behavior with concrete values. |
| Byte-enabled write (`scratchpad_ram_tb.cpp:255-266`) | Genuine/Partial | Proves one 8-byte alternating mask only; read masks and repeating masks are absent. |
| Hex preload (`scratchpad_ram_tb.cpp:199-217`) | Partial | Expected words are parsed from the same fixture at runtime, weakening the oracle. |
| Debug transport and debug APIs (`scratchpad_ram_tb.cpp:288-319`, negative bench `310-324`) | Coverage-only | Backdoors mirror internal storage and defensive guards. |
| ECC (`scratchpad_ram_tb.cpp:322-345`, negative bench `330-358`) | Partial | Error creation is necessarily a debug hook, but scrub removal and all-disabled writes are not independently proved. |
| TLM negatives (`scratchpad_ram_tb.cpp:348-369`) | Genuine/Partial | Covers range/alignment/width/streaming/command, not null pointers or byte-enable edges. |
| Preload fatal/format branches (`scratchpad_ram_neg_tb.cpp:170-308`) | Partial/Coverage-only | Useful parser checks, but much of the bench exists to hit constructor branches and accepts any exception. |
| CCI/dump (`scratchpad_ram_tb.cpp:371-409`) | Partial/Coverage-only | Handle mutation and text smoke do not verify next-transaction timing. |

No private-public macro, touch-all-offset loop, or test-only source branch was found.

## Artificial coverage mechanisms

- Public direct-memory backdoors are used extensively: `dbg_read64`, `dbg_read32`, `dbg_load_bytes`, ECC hooks, and `dump_state`.
- The preload oracle re-reads the same input file (`test/scratchpad_ram_tb.cpp:202-215`) instead of using fixed independently derived expected bytes.
- Fatal helpers accept any exception (`test/scratchpad_ram_neg_tb.cpp:119-128`).
- `SC_ERROR` is demoted to display-only (`test/scratchpad_ram_tb.cpp:444-445`, `test/scratchpad_ram_neg_tb.cpp:158-160`).

## Findings

1. **High — legal-shaped transactions with a null buffer can crash.** `b_transport` and `transport_dbg` dereference/copy through `data_ptr` without a null check (`src/scratchpad_ram.cpp:239-313, 324-340`).
2. **High — the preload test uses a non-independent expected source.** It parses expected values from the exact fixture being loaded (`test/scratchpad_ram_tb.cpp:202-215`), so coordinated parser/fixture mistakes and endianness assumptions can escape.
3. **Medium — all-disabled byte-enable writes clear ECC state.** Every write calls `ecc_clear_range` even if no byte is enabled (`src/scratchpad_ram.cpp:289-301`). No test checks that a no-op mask remains a no-op to data and ECC.
4. **Medium — byte-enabled reads are untested.** The implementation leaves disabled destination bytes unchanged (`src/scratchpad_ram.cpp:275-283`), but the suite covers writes only.
5. **Medium — debug transport does not enforce command, streaming, or byte-enable behavior.** An ignore command returns `length` without transferring anything (`src/scratchpad_ram.cpp:324-340`).
6. **Medium — binary preload has no successful-path test.** The negative bench covers missing/empty binary files only (`test/scratchpad_ram_neg_tb.cpp:194-220`).
7. **Medium — invalid `init_file_format` is silently accepted and tested as success.** `resolve_format` treats any typo as auto (`src/scratchpad_ram.cpp:413-422`; `test/scratchpad_ram_neg_tb.cpp:296-308`) despite metadata documenting `hex|bin|auto`.
8. **Medium — mutable delay behavior is not measured.** The handle is mutated, then only the handle value is checked (`test/scratchpad_ram_tb.cpp:375-396`); the later delay test checks merely `> 0` (`test/scratchpad_ram_tb.cpp:413-429`).
9. **Closed (2026-09-24) — negative-bench process failures are no longer hidden under ASan.** Combined child exits fail the ASan phase. SMC orchestrator ASan PASS.
10. **Low — AXI extension and DMI query behavior are absent.** Successful transactions set `dmi_allowed(false)`, but no `get_direct_mem_ptr` contract or extension-present test exists.
11. **Low — report demotion can hide unrelated SystemC errors.** Both benches use display-only `SC_ERROR` handling without feeding such reports into `g_failures`.

## Missing scenarios

- Fixed byte-level preload oracle for little-endian memory representation.
- Successful binary and auto-detected uppercase-suffix preloads; short binary with zero-filled tail; oversized binary policy.
- All transfer widths at address 0 and highest legal aligned address.
- Byte-enabled reads, repeating BE lengths, all-disabled masks, partial writes across ECC granules where legal, and BE pointer with zero length.
- Null data pointer; huge/OOB addresses; streaming width greater than length; exact response classes.
- Correctable error read twice to demonstrate scrub, uncorrectable errors in each overlapped granule, disabled writes preserving ECC, enabled writes clearing it.
- Reset while ECC flags exist and while accesses occur; establish whether injected ECC state is retained.
- Exact mutable access delay, explicit DMI denial, AXI extension present.
- Debug command/range/alignment/BE/streaming matrix.

## Proposed testcases

| ID | Stimulus | Expected result | Path exercised | Feature proved |
|---|---|---|---|---|
| SPM-PRE-001 | Load a small hex image and compare frontdoor bytes against hard-coded constants. | Exact byte order and zero tail independent of loader/parser. | hex preload + read | Independent preload oracle |
| SPM-PRE-002 | Load valid `.bin`/`.IMG` images with known bytes and a short tail. | Auto format works; bytes and zero-fill are exact. | binary loader/auto suffix | Binary preload |
| SPM-FD-001 | Read/write 1/2/4/8 bytes at first and last legal aligned locations. | Exact round-trip; first crossing transfer returns address error. | `b_transport` range/alignment | Full transfer range |
| SPM-BE-001 | Use BE lengths 1, 2, and full length on reads and writes. | Mask repeats per TLM rules; disabled destination/data bytes stay unchanged. | BE loops | Byte enables |
| SPM-BE-002 | Inject uncorrectable ECC, issue an all-disabled write, then read. | Error remains; no-op write has no state effect. | write/ECC interaction | Masked-write correctness |
| SPM-ECC-001 | Inject correctable error, read twice, then inject errors in each touched word. | First read scrubs; subsequent reads are clean; any uncorrectable overlap fails. | `ecc_check` | ECC granularity/scrub |
| SPM-TLM-001 | Command/length/alignment/range/null/streaming matrix. | Deterministic errors and no memory access on rejects. | validation | Protocol safety |
| SPM-TLM-002 | Query DMI and transact with canonical AXI extension. | DMI explicitly denied; sideband does not corrupt access. | TLM integration | DMI/AXI contract |
| SPM-DBG-001 | Legal and malformed debug read/write/ignore requests. | Only explicit legal operations return bytes; no side effects on rejection. | `transport_dbg` | Debug protocol |
| SPM-RST-001 | Write data and inject ECC, pulse reset, then read. | Data retention and documented ECC-state retention/reset policy are both proved. | `reset_proc` | Complete reset semantics |
| SPM-CCI-001 | Mutate delay from 2 ns to 7 ns and measure consecutive transactions. | Annotated delays are exactly 2 ns then 7 ns. | mutable CCI | Timing behavior |
| SPM-CFG-001 | Supply an unsupported format string. | Exact fatal configuration error rather than typo fallback. | format validation | Configuration safety |

## Verdict

**Good basic RAM coverage, but the oracle and edge semantics need repair.** Frontdoor width, reset-retention, and one byte-mask case are genuine. The ASan negative-bench exit hole is **closed (2026-09-24)**. Null-buffer safety, independent preload expectations, byte-enable read/no-op behavior, and binary preload remain sign-off gaps; debug-heavy coverage should not substitute for them.
