# SMC Boot ROM Test Audit and Plan

## Files audited

- Model: `smc/peripherals/bootrom/include/bootrom.h`, `src/bootrom.cpp`
- Tests/fixtures: `test/bootrom_tb.cpp`, `bootrom_bin_tb.cpp`, `bootrom_neg_tb.cpp`, `test/fixtures/*`
- Build/coverage: `CMakeLists.txt`, `test/CMakeLists.txt`, `run_tests.sh`
- Shared coverage/ASan gates invoked by the runner.

## Behavior inventory

- CCI: immutable size/file/format; mutable access delay.
- Port/process: target socket, active-low reset input, no-op reset method.
- Memory contract: zero fill; hex and raw binary preload; 1/2/4/8-byte naturally aligned reads; write-ignore with OK response.
- Validation: width, alignment, streaming width, byte enables, range, command.
- TLM: `b_transport`, `transport_dbg`, delay annotation, DMI denial.
- Debug/backdoors: 32/64-bit reads, arbitrary byte loader, size query, state dump.
- Preload helpers: case-insensitive suffix, auto format selection, line parser, zero-only oversized-hex tolerance, binary truncation.

## Existing scenario matrix

- **Genuine** — Bus reads validate hex preload, subword slicing, zero tail, write-ignore, reset persistence, malformed request statuses, mutable delay presence, and CCI handles (`bootrom_tb.cpp:194-520`).
- **Genuine** — Separate binary-preload test compares bus/debug words against the fixture and checks zero padding/write-ignore (`bootrom_bin_tb.cpp:102-162`).
- **Genuine** — Constructor/preload failures cover invalid size/delay, missing/empty files, malformed/no-data/oversized nonzero hex (`bootrom_neg_tb.cpp:180-307`).
- **Partial** — Expected preload words are parsed from the same input fixture at runtime. This proves loading consistency but not an independent format/endian oracle (`bootrom_tb.cpp:200-224`, `bootrom_bin_tb.cpp:106-124`).
- **Partial** — Delay is asserted only as nonzero after a prior CCI mutation; the exact additive value is not checked (`bootrom_tb.cpp:501-521`).
- **Partial** — Reset method is structurally exercised, but because it is intentionally empty there is no event/check proving invocation versus mere content persistence.
- **Coverage-only** — The negative bench announces line-coverage completion and repeats every `b_transport` arm because each binary carries another linked copy (`bootrom_neg_tb.cpp:4-29,461-512`).
- **Coverage-only** — Tests intentionally accept invalid `init_file_format="garbage"` and parser oddities such as bare `0x`, matching implementation permissiveness rather than a documented valid-value contract (`bootrom_neg_tb.cpp:349-362,416-432`).
- **Coverage-only** — Dump checks only search for labels; they execute formatting rather than validate memory behavior (`bootrom_tb.cpp:488-498`).

No private-access macro, direct private method call, or offset sweep was found. Public debug loading/read APIs are used heavily.

## Findings

- **High — null data pointer can crash.** `b_transport` and debug reads use `memcpy` without checking `buf` (`bootrom.cpp:193-241,253-267`). No test submits null.
- **High — `transport_dbg` accepts unsupported commands.** It copies only for reads, then returns `length` for writes and also for IGNORE/other commands (`bootrom.cpp:263-267`). The tests cover debug read/write and malformed geometry but not unknown command.
- **High — default and ASan runners omit two registered tests.** Plain `run_tests.sh` and `--asan` execute only `bootrom_tb`; `bootrom_bin_tb` and `bootrom_neg_tb` run only in coverage or `--ctest` (`run_tests.sh:149-220,222-303,354-360`). Binary loader and fatal/parser paths therefore are not release/ASan gated.
- **Medium — invalid format is silently accepted despite advertised valid values.** `resolve_format` falls through to auto detection for unknown strings (`bootrom.cpp:324-332`), and the test requires this behavior (`bootrom_neg_tb.cpp:349-362`). This can hide a misspelled production configuration.
- **Medium — range arithmetic can wrap.** `addr + length > size` is used without overflow-safe subtraction (`bootrom.cpp:217-221,261`). A near-`UINT64_MAX` address can wrap, pass the second clause after the first is evaluated false only when size is also extreme; immutable size is not bounded to host/vector limits.
- **Medium — mutable delay is not reflected in diagnostics.** `b_transport` refreshes `access_delay_`, but `dump_state` prints stale constructor-cached `cfg_.access_delay_ns` (`bootrom.cpp:237-242,299-307`).
- **Medium — binary truncation and hex oversize semantics lack strong boundary tests.** A binary larger than ROM is silently truncated (`bootrom.cpp:380-389`); only a smaller production fixture is tested. Hex extra zeros are accepted while nonzero words fatal, but no large/mixed boundary corpus exists.
- **Medium — host endianness is embedded in hex loading.** Parsed `uint64_t` is copied in host byte order (`bootrom.cpp:433-435`) while tests derive expectations as host integers, so both can pass together on little-endian systems.
- **Medium — DMI contract is incomplete.** Successful accesses set `dmi_allowed(false)` but no `get_direct_mem_ptr` callback/test exists (`bootrom.cpp:242`).
- **Medium — canonical SMC AXI extension is included but never read or tested.** Unlike the other audited SMC targets, `b_transport` does not extract the canonical extension at all.
- **Low — fatal matcher accepts any exception.** Constructor tests do not verify report ID/message, so unrelated failures can pass (`bootrom_neg_tb.cpp:121-129`).
- **Low — test globally demotes `SC_ERROR`.** Primary and negative tests suppress normal fatal-on-error policy to probe CCI immutability (`bootrom_tb.cpp:536-541`, `bootrom_neg_tb.cpp:161-171`), which can conceal unrelated SystemC errors in the same binary.

## Missing scenarios

- Null pointer, unknown debug command, debug streaming width/byte enable policy.
- Exact response, buffer preservation, delay, and DMI outcome for every valid/error request.
- Attached canonical AXI extension with all fields and preservation check.
- Independent known byte arrays for hex and binary, including explicit endian expectations.
- Binary exactly empty, 1 byte, 7/8/9 bytes, exact ROM size, and larger-than-ROM truncation.
- Hex maximum value, overflow numeric token, CRLF/tabs, inline comments, mixed zero/nonzero overflow, very long line, and unreadable midstream errors.
- Very small valid ROM and highest legal address for all widths.
- Address arithmetic near `UINT64_MAX` and impractically large size allocation failure policy.
- CCI delay exact mutation and diagnostic consistency.
- `get_direct_mem_ptr` denial.

## Proposed tests

1. **Independent preload oracle:** create fixtures with literal byte sequences and hard-coded expected bytes/words. Check every byte through bus reads; do not parse expected data with the DUT's format assumptions.
2. **Binary boundary matrix:** sizes 1, 7, 8, 9, ROM-size, and ROM-size+1. Expect zero padding or documented truncation exactly and verify terminal bytes.
3. **Hex parser contract:** explicitly decide whether invalid format names and bare `0x` are fatal. Test report severity/message and exact accepted grammar.
4. **TLM matrix:** null pointer, command, width, alignment, streaming width, byte enables, last legal access, OOB, and overflow addresses. Expect exact status, unchanged buffer/content on errors, additive delay policy, and DMI false.
5. **Debug/DMI:** read/write/IGNORE commands plus malformed pointer/geometry. Expect read byte count only for supported commands, write-ignore without mutation, zero for unsupported commands, and DMI denial.
6. **AXI extension:** attach a fully populated canonical extension to valid and invalid accesses; expect no field mutation.
7. **Mutable delay:** seed 5 ns, expect exactly `5 + configured` on success, mutate CCI, and repeat. Verify `dump_state` reports the live value or document snapshot semantics.
8. **Reset observation:** read before/during/after a real reset edge and prove identical data/status/delay, while a monitor confirms the process edge occurred.
9. **Runner regression:** run all three binaries in Release, ASan, and Coverage; fail on any binary exit or sanitizer report.

## Verdict

**High risk.** Core ROM reads and preload formats have substantial tests, but protocol safety, debug command correctness, independent endian/format validation, and DMI/AXI coverage are missing. Two of three binaries are excluded from normal and ASan runs, and several tests exist principally to satisfy linked-copy coverage.
