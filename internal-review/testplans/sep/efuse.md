# eFuse SystemC/TLM Test Audit and Test Plan

## Scope and evidence basis

Static audit only. No test executable or coverage report was run.

Audited files:

- Model: `include/efuse.h`, `efuse_base.h`, `efuse_register.h`, `src/efuse.cpp`, `src/efuse_base.cpp`
- Tests: `test/inc/testbench.h`, `efuse_basetest.h`, `efuse_test.h`, `test/src/testbench.cpp`, `test_coverage.cpp`, `efuse_test.cpp`, `efuse_basetest.cpp`
- Configuration/build: `config/accellera_config.ini`, `config/default_efuse.preload`, `CMakeLists.txt`, `run_tests.sh`
- Shared inherited transport: `common/include/reg_file.h`

## Model behavior inventory

- Main eFuse shadow/MMR target and separate shim-control target.
- 8192-bit OTP array, preload from bit-per-line or readmemh-like image, parameter-based initialization, one-step sensing at elaboration and shadow caches.
- Extensive RO fuse fields plus WOSET locks, lifecycle/disable/revoke/version shadows.
- Per-field write/read locks across scalar/vector regions; denied shadow read returns `DENY_WORD`, denied write is dropped, both pulse locked-field IRQ.
- OTP program/read commands with enable/go/status/readback, one-way bit programming, lock/token/address checks and sticky request/address errors with W1S clear controls.
- Lifecycle differential encoding and token-gated legal transition machine, transient RMA and prod-debug freeze.
- SHA-256 token comparison for SIP, chiplet and security-disable inputs; hardware-written match status.
- Consumer accessors and shadow-change callback; secure-test strap blocks OTP commands and zeros key-manager UID view.
- CCI parameters for fuse/default/strap values and preload path.
- Program/read/token operations complete inside the initiating write; no busy interval or modeled macro timing.
- Inherited target transport supports partial/BE/debug operations, always returns OK, ignores streaming width and provides no DMI.

## Existing scenario-to-function matrix

| Existing scenario | Model functions/paths reached | Quality |
|---|---|---|
| Parameter fuse load and array fields | load/sense path and RO reads | Genuine |
| RO protection and WOSET locks/disables/version arrays | register masks and callbacks | Genuine |
| Program/read enable/error/status/readback and one-way programming | OTP command callbacks | Genuine |
| Preload good/bad/missing/whitespace and atomic replacement | preload parser/sense | Genuine, via public backdoor API |
| Lock enforcement, sentinel, IRQ and OTP guard distinction | lock callbacks and IRQ process | Genuine |
| Token hash/match/mismatch/order, RO result and LC bit gating | token and program paths | Genuine |
| Lifecycle legal/refused/token/transient/prod-debug transitions | transition machine | Genuine |
| Accessors/callbacks | integration API | Partial |
| Separate shim window | dual target sockets | Genuine |
| Secure test strap | elaboration config, command block and secret zeroing | Genuine/Partial |

## Artificial coverage shortcuts and weaknesses

1. **High — a public backdoor repeatedly resets state to make later coverage reachable.** `test/src/test_coverage.cpp:460-540`, `:599-617`, `:744-900` and later transition tests call `preload_fuses_from_file()` after elaboration. The API is documented for test/platform setup, but using it mid-run bypasses OTP programming, reset and lock irreversibility. These are valid unit setups, not end-to-end fuse lifecycle tests.
2. **High — consumer accessor test contains an unconditional pass.** `test_coverage.cpp:1193-1196` calls `get_security_disable()` and `get_secure_tm()` only to execute them, then reports pass without checking either result.
3. **Medium — “coverage” tests are actually the majority of functional verification.** `testbench.cpp:469-476` always runs `run_coverage_tests()`. Naming is misleading; tests 9–27 are substantial behavior tests and should be first-class scenarios rather than coverage fillers.
4. **Medium — secure-mode test codifies an intentional timing divergence.** `src/efuse.cpp:963-969` states RTL stalls until timeout/forever but the model completes immediately; `testbench.cpp:400-439` asserts immediate done/no-error. This proves model behavior, not hardware-equivalent temporal behavior.
5. **Medium — configuration expected values are read from DUT parameters.** `testbench.cpp:92-120` and array checks at `:135-164` use the same CCI parameters consumed by the DUT. This proves parameter plumbing, but not that a fixed independent configuration is mapped to the right offsets unless the config itself is independently asserted.
6. **Medium — direct accessor tests bypass bus-visible policy by design.** `test_coverage.cpp:391-426`, `:1170-1237` directly call integration APIs. Useful for consumers, but those lines cannot establish TLM access semantics.
7. **Low — CTest runs only the default strap.** `CMakeLists.txt:185-186` registers one no-argument test. The shell runner and coverage target run secure mode too, but plain `ctest` does not.
8. **Low — ASan runner lacks explicit log/leak gating.** `run_tests.sh:66-94` executes both configurations but does not inspect sanitizer logs.

No private-public macro or direct private callback calls were found.

## Missing or inadequately proven scenarios

- Main and shim target unsupported commands, null pointer, zero length, aperture boundaries, cross-window addresses and exact response statuses.
- Partial/unaligned and byte-enabled accesses to WOSET, control, token and status-clear registers; side-effect callbacks do not receive byte-enable masks.
- Streaming width and debug transport side-effect policy; DMI behavior.
- Program/read busy/done temporal behavior, timeout registers, timeout=0/finite timeout, and secure-test stall/timeout behavior.
- Reset/power-cycle semantics for shadow flops, lock state, sticky errors, token matches and interface control. The model has no reset port/process.
- Two locked accesses within, before and after the 1 ns IRQ pulse; exact extension/merge semantics and reset.
- All lock regions/banks, every vector boundary and lock-bit mapping. Current enforcement focuses on BL1 and selected fields.
- Read/write lock behavior on every software-writable field and OTP equivalent.
- Hex preload origin markers/comments/truncation/overflow/empty file and relative `SEP_VP_INI_DIR` resolution.
- SHA/token known-answer vectors beyond the all-zero reference and both nonzero word orders.
- Security-disable result with matching digest and both revision-enable values.
- Shadow-change callback for every changing field and no callback on refused/no-op writes.
- Simultaneous TOKEN_EOP bits and interaction among transient RMA results.
- Lifecycle invalid encoded preload, every terminal state, every legal transition and upper-bit writes.
- Multi-initiator concurrent accesses and program/read overlap.

## Proposed testcases

| ID | Setup / stimulus | Required assertions | Paths | Feature |
|---|---|---|---|---|
| EFU-TLM-001 | IGNORE/null/zero/OOB/cross-window main/shim payloads | Exact response, no mutation/IRQ/status | inherited targets | Command/response/address/length |
| EFU-TLM-002 | Partial/unaligned/byte-enable writes to LOCKS, CTRL, STATUS and TOKEN_EOP | Only enabled lanes act; no unintended clear/go/token | callbacks | Byte enable/streaming |
| EFU-TLM-003 | Debug read/write lockable and side-effect offsets; DMI request | Explicit side-effect policy/byte count; DMI false | debug path | Debug/DMI |
| EFU-TIME-001 | Program/read with timeout disabled and finite values; secure_tm on/off | Busy/done/error and elapsed time match documented abstraction | command callbacks/timeouts | Temporal behavior |
| EFU-RST-001 | Add/apply modeled reset/power cycle after locks/errors/matches | Shadow-reset versus OTP persistence exactly defined | reset/sense | Reset/persistence |
| EFU-LOCK-001 | Table-drive every lock region first/last word and all banks | Correct sentinel/drop/IRQ and neighboring region unaffected | lock map | Boundary/policy |
| EFU-IRQ-001 | Issue denials separated by 0, 0.5, 1 and 2 ns | Pulse merge/extension/count exact | IRQ process/events | SystemC timing |
| EFU-PRE-001 | Fixed fixtures for both image formats, origins/comments/truncation/errors | Independently expected fuse/shadow words and atomic failure | preload parser | File/config behavior |
| EFU-TOK-001 | Independent nonzero token/digest KAT for all three comparators | Exact match/mismatch, word order and result independence | SHA/token callbacks | Crypto/register |
| EFU-SEC-001 | Matching security-disable token with revision enabled/disabled | `get_security_disable()` exact true/false; callback behavior | consumer accessor | Integration semantics |
| EFU-LC-001 | Matrix every valid source/write/token/prod-debug combination | Exact encoded next state or refusal; no broad alternatives | LC transition | FSM/security |
| EFU-CONC-001 | Overlap read/program/token requests from two initiators | Defined serialization/status/data without lost IRQ | target callbacks | Concurrency |

## Coverage quality verdict

**Verdict: Generally strong semantic tests with localized backdoor and temporal gaps.** Most “coverage” scenarios have concrete assertions and independent state checks, so this IP is substantially better than the other audited coverage-only suites. Weaknesses are the unconditional accessor pass, heavy mid-run preload backdoor use, missing TLM protocol cases, and explicit collapse of hardware command timing. Coverage wiring runs default and secure configurations for the coverage target, but no measured percentage was inspected.
