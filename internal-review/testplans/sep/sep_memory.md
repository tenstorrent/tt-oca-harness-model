# SEP `sep_memory` test audit

## Scope and audited files

- Model: `model/inc/sep_memory.h`, `model/src/sep_memory.cpp`
- Storage dependency: `sep/utils/paged-memory/paged_mem.h`
- Build/test: `CMakeLists.txt`, `run_tests.sh`, `doc/test_plan.adoc`
- No standalone test source exists.

## Behavior inventory

- `load_data`, `load_zero`, and `load_binary_file` populate sparse paged memory.
- `b_transport` supports READ and WRITE, repeating byte enables, zero-length transfers, ROM write-ignore behavior, a fixed `+10 ns` annotated delay, and rejects non-null-length transfers with a null data pointer.
- Unsupported commands terminate through `sc_assert`.
- `get_direct_mem_ptr` grants one allocated page, beginning at the requested address, with read-only or read/write permission.
- `transport_dbg` performs untimed reads/writes and returns the requested length.
- Unallocated bytes read as zero; writes allocate 4 KiB pages.

## Existing test classification

| Area | Standalone evidence | Classification |
|---|---|---|
| Build/link | `run_tests.sh` builds `libsep_memory_model.a` | Compile-only |
| Functional bus behavior | None | Missing |
| Debug/DMI | None | Missing |
| ROM semantics | Only claimed as VP firmware coverage in `doc/test_plan.adoc:42-55` | Indirect |
| Coverage/ASan | No executable, no instrumented mode | Missing |

## Coverage-shortcut findings

1. **Critical — no executable coverage at all.** `run_tests.sh:4-5,58-60` explicitly makes this a build-only check, while `CMakeLists.txt:47-66` defines only a static library. Every model branch can regress without failing the IP test.
2. **High — platform claims are not a measurable unit-test substitute.** `doc/test_plan.adoc:8-14,38-55` delegates behavior to firmware but provides no per-function assertions, negative protocol cases, coverage report, or sanitizer run.
3. **High — direct-loader behavior is unverified.** Public backdoor methods at `sep_memory.cpp:26-52` are not tested for page boundaries, empty files, short files, or preservation of surrounding bytes.
4. **High — debug path has unsafe untested assumptions.** `transport_dbg` dereferences `ptr` while formatting before checking it (`sep_memory.cpp:160-166`) and ignores `m_read_only` on writes (`:173-175`), unlike `b_transport` (`:82-103`). No test establishes whether that divergence is intentional.
5. **Medium — DMI range/pointer contract is unverified.** `get_direct_mem_ptr` returns a pointer offset into a page but an end address at page end (`sep_memory.cpp:130-149`); no initiator checks address-to-pointer translation, permissions, or unallocated-page refusal.
6. **Medium — coverage and ASan gates cannot run.** `run_tests.sh:6-12` has neither `--coverage` nor `--asan`; this IP is also explicitly skipped by `sep/peripherals/run_all_peripherals.sh:4-5,57-59`.

No `private`→`public` macro, report suppression, or test-only model branch was found; the stronger issue is absence of tests.

## Missing scenarios

- Frontdoor reads/writes at lengths 0, 1, 2, 4, 8, 16, 4095, 4096, and cross-page lengths.
- Byte-enable patterns: all disabled, alternating, repeating enable length shorter than data, and non-null pointer with zero byte-enable length.
- Null data pointer with zero and nonzero lengths; unsupported and ignore commands.
- Streaming width less than/equal/greater than data length (currently ignored).
- Initial delay preservation and exact `+10 ns`; error paths must not add delay.
- ROM frontdoor and debug writes, and DMI write permission.
- DMI before allocation, at page first/last byte, pointer translation, page end, and permission.
- Debug read/write return counts, cross-page access, null pointer, unsupported command, and no timing annotation.
- `load_zero`/`load_data` at page edges; binary load missing/empty/valid/oversize inputs.
- 32-bit address truncation in `b_transport`/`transport_dbg` (`unsigned addr` at `sep_memory.cpp:56,156`) versus 64-bit loader/DMI addresses.

## Proposed testcases

| ID | Stimulus | Expected result | Path | TLM feature |
|---|---|---|---|---|
| MEM-001 | Frontdoor write/read lengths 0,1,4,16 | Exact data, OK, delay +10 ns | `b_transport` | command, length, delay |
| MEM-002 | Transfer crossing 4 KiB boundary | Both pages correct; untouched bytes zero | `b_transport` | length/address boundary |
| MEM-003 | Repeating byte-enable masks and `be_len=0` | Enabled lanes update; zero BE length returns BYTE_ENABLE_ERROR | `b_transport` | byte enables |
| MEM-004 | Null pointer with len 0 and len 1 | len 0 succeeds; len 1 returns GENERIC_ERROR without dereference | `b_transport` | data pointer |
| MEM-005 | READ/WRITE with streaming widths 0,1,len,len+1 | Documented policy enforced; unsupported geometry rejected if required | `b_transport` | streaming width |
| MEM-006 | ROM write then read; repeat through debug | Frontdoor remains unchanged; debug behavior explicitly asserted | both | ROM/debug |
| MEM-007 | DMI before/after allocation at page edges | Refuse unallocated; correct pointer/range/permissions when allocated | `get_direct_mem_ptr` | DMI |
| MEM-008 | Debug read/write, cross-page and zero length | Correct byte count/data, no delay | `transport_dbg` | debug transport |
| MEM-009 | Unsupported command | Deterministic report/error policy, no memory mutation | both | command |
| MEM-010 | Loader data/zero/file across page boundaries | Exact contents and unaffected neighbors | load API + frontdoor verify | backdoor-to-frontdoor |
| MEM-011 | Address above 4 GiB | No truncation/aliasing, or explicit address error | frontdoor/debug | 64-bit address |
| MEM-012 | Add standalone release/ASan/coverage targets | Both functional tests execute; model lines ≥95%; sanitizer clean | build system | quality gate |

## Verdict

**Fail / critical coverage gap.** The model has substantial TLM, DMI, debug, timing, ROM, and loader behavior but no standalone execution or coverage. VP firmware is insufficient to validate protocol errors and boundary behavior. A frontdoor-first unit bench and separate ASan/coverage runs are required before coverage can be considered meaningful.
