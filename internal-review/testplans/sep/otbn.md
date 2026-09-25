# otbn Test Audit and Plan

## Audited files

- Model: `include/otbn.h`, `otbn_base.h`, `otbn_register.h`, `otbn_interfaces.h`, `src/otbn.cpp`, `src/otbn_base.cpp`.
- Algorithms: every implementation/header under `algo/`, including RSA-2048, key-enabled RSA-2048, RSA-3072, P-256 ECDSA, summation, loop, smoke, RND, and callback_cov.
- Tests: `test/inc/otbn_basetest.h`, `test/inc/otbn_test.h`, `test/inc/testbench.h`, `test/src/otbn_basetest.cpp`, `otbn_test.cpp`, `testbench.cpp`, `test_coverage.cpp`.
- Build/coverage: `CMakeLists.txt`, `run_tests.sh`.

## Behavior inventory

- Main CSR/IMEM/DMEM target uses 32-bit `regmodel::Memory`; key manager has a separate target socket.
- State machine covers boot internal wipe, IDLE, execute, DMEM/IMEM wipe, and terminal LOCKED.
- Commands execute algorithms or wipe memories; invalid/BUSY/LOCKED commands have distinct silent-ignore behavior.
- Interrupt state/test/enable and deferred `intr_done`; alert test, recoverable pulse, sticky fatal alert.
- CTRL changes software-error fatality; ERR_BITS is sticky W1C in IDLE/LOCKED; fatal cause persists until reset; INSN_CNT has state-dependent clear; LOAD_CHECKSUM updates on memory writes.
- Host IMEM is 8 KiB; host DMEM is first 3 KiB of a 4 KiB algorithm buffer. Busy/locked/protected accesses have state-specific block, zero, error, wipe, alert, and lock behavior.
- Algorithm execution copies DMEM, waits cycle count, executes, copies results back, records count, wipes internal WDR state, and handles recoverable/fatal status.
- Key manager maps four key regions plus KEY_CTRL, adds 10 ns, rejects reads/unknown commands/bad addresses, zero-pads high key halves, and controls validity.
- OTP mock response, OTP wipe request interface, LC escalation/RMA monitor and acknowledgments, WDR/CSR/RND callback interfaces.
- Outgoing key manager/CSR TLM robustness beyond normal payloads is implemented only partially.

## Existing tests matrix

### Genuine

- The main suite exercises reset, state transitions, command execution/wipes, interrupts/alerts, IMEM/DMEM windows, protected region, busy/locked access, checksum, lifecycle escalation/RMA, key programming, and algorithm-specific paths.
- Most architectural behavior is driven through CSR or key-manager sockets and checked through independent visible registers, memory, or signals.
- Algorithm outputs include fixed arithmetic cases and OpenSSL cryptographic checks in several algorithm tests.
- Key manager read/ignore/address errors use the actual socket and assert exact response.
- The multi-algorithm coverage runner executes all recognized algorithms plus unknown fallback. **Fail-closed as of 2026-09-24:** a failed algorithm aborts merge. RSA-vector tests run only when `uses_rsa2048_semantics()` is true (`rsa_2048` / `unknown_algo`).

### Partial

- Many main-suite failures are only warnings/notes. Examples include unchanged execution result and nonzero ERR_BITS (`test/src/testbench.cpp:1790-1798,1831-1838`), interrupt output timing (`7490-7497,7520-7529`), unexpected state/error persistence (`7605-7612,7626-7630,7651-7658,7701-7708,7757-7766`), and recoverable alert/error checks throughout lines 2913-4008.
- `wait_for_idle()` and `wait_for_algorithm_completion()` only log timeout and return void (`test/src/testbench.cpp:267-299`); callers can continue and pass after a timeout.
- RSA verification contains permissive “result area changed”/“find expected byte anywhere” logic in some paths rather than exact full-buffer comparison (`test/src/testbench.cpp:13109-13139`).
- Key invalidation test verifies only that KEY_CTRL write returned OK, not that a subsequent key-WDR access fails (`test/src/test_coverage.cpp:74-99`).
- Several coverage algorithm negatives report only that a path executed, without asserting the specific error/status/output, e.g. invalid P-256 pubkey and RSA-3072 zero modulus (`test/src/test_coverage.cpp:323-347,379-398`).

### Coverage-only

- **Direct private access is deliberately enabled:** `otbn_ip` declares `friend class testbench` (`include/otbn.h:257-259`).
- `test_cov_imem_oob_and_busy_block()` directly assigns `current_state` and directly invokes IMEM/DMEM/ERR_BITS callbacks (`test/src/test_coverage.cpp:559-624`), bypassing both sockets and legal state transitions.
- `test_cov_keymgr_ignore_and_invalid_cmd()` directly invokes `cmd_write_callback()` and pokes state (`test/src/test_coverage.cpp:629-679`).
- `test_cov_algorithm_standalone_error_paths()` directly invokes model CSR/RND handlers and standalone algorithm objects (`test/src/test_coverage.cpp:761-893`).
- `otbn_algorithm_callback_cov` exists specifically to touch CSR/WDR callback branches; it ignores callback return values and always writes marker/succeeds (`algo/otbn_algorithm_callback_cov.cpp:13-52`). The marker proves execution, not callback semantics.
- Algo-specific smoke/loop/callback_cov branches in `run_tests()` execute but do not assert their documented result (`test/src/testbench.cpp` algo-specific pass).
- ~~Coverage runner ignores failing test processes.~~ **Closed 2026-09-24.** Workers return nonzero; merge is refused. Measured `--coverage`: **99.2%** (1322/1332), all 10 algorithms succeed.

## Shortcut findings

- **Closed (2026-09-24) — coverage no longer accepts failing binaries.** `run_tests.sh` refuses to merge if any algorithm worker exits nonzero. RSA-vector cases are gated off non-RSA algorithms so those workers can pass without gold-filing RSA oracles.
- **High — friend/private state and callback bypass:** `include/otbn.h:257-259`; `test/src/test_coverage.cpp:559-679`. These branches do not prove externally reachable behavior.
- **High — synthetic callback_cov algorithm:** it calls callbacks and ignores failures, then returns success (`algo/otbn_algorithm_callback_cov.cpp:13-52`).
- **High — permissive warnings:** multiple architecturally required results, errors, states, and outputs do not fail the suite; representative lines are listed above.
- **High — timeout helpers do not fail:** `test/src/testbench.cpp:267-299`.
- **Medium — expected CRC duplicates implementation:** the test reproduces the model's byte packing, polynomial, and bit loop almost line-for-line (`test/src/testbench.cpp:2805-2840`; model `src/otbn.cpp:1265-1302`). A shared defect can pass both.
- **Medium — exact algorithm output sometimes replaced by permissive checks:** “modified area” or expected byte anywhere is insufficient.
- **Medium — key manager malformed payloads absent:** `keymgr_b_transport()` dereferences `ptr` for KEY_CTRL and iterates it for data with no null/length/byte-enable/streaming checks (`src/otbn.cpp:1676-1749`).
- No `#define private public` macro was found; the friend declaration is the equivalent access shortcut.

## Missing scenarios

- Main CSR TLM2: bad/ignore command, unmapped/unaligned address, null pointer, zero/short/oversized length, byte enables, streaming width, debug/DMI, annotated delay, response behavior for blocked callbacks.
- Key manager: null pointer, zero length, partial writes crossing region boundaries, KEY_CTRL lengths other than four, byte enables, streaming width, DMI/debug, address edge `0x5f/0x60/0x61`, exact 10 ns accumulation, extension behavior if required.
- State transitions: exact timeout failures, reset at each busy state and during alert pulse, simultaneous LC escalation/RMA, request deassertion/reassertion, acknowledgment deassertion, commands during each state.
- Memory: every IMEM/DMEM first/last/protected boundary, unaligned/subword/byte-enable accesses, checksum on DMEM as well as IMEM, rejected writes leaving checksum unchanged.
- Interrupt/alerts: every enable ordering, W1C zero/one/reserved masks, reset with pending deferred events, exact recoverable pulse duration, sticky fatal behavior.
- Key/WDR: exact loaded words including zero padding, invalidation followed by access, wipe invalidates key, general WDR access without key, invalid index and null data safely where directly API-testable.
- Algorithms: exact full outputs and exact error bits for every positive/negative/boundary vector; DMEM minimum exact size and one byte short; zero/max operands; deterministic RND-seed vector; instruction/cycle counts.
- OTP response exact key/nonce/seed values and request count for every wipe path.

## Proposed cases

1. ~~**Make coverage fail closed.**~~ **Closed 2026-09-24.**
2. **Socket-only busy/locked access matrix:** reach each state through command/LC/reset inputs, issue IMEM/DMEM transactions through `target_socket`, and assert response, returned zero, ERR_BITS, fatal cause, alert, wipe, and final state. Do not assign `current_state`.
3. **Strict completion helper:** polling returns bool/fails test on timeout. Every command case asserts the observed BUSY state, exact completion time window, final state, done state, and output.
4. **CSR malformed TLM suite:** raw transactions cover command/address/length/pointer/BE/streaming combinations with exact response and no state mutation.
5. **Key manager protocol suite:** capture pre/post WDR behavior through an algorithm that returns callback status semantically; test exact key bytes, high-half zero padding, partial boundary writes, KEY_CTRL validation/invalidation, response, and 10 ns delay.
6. **Independent checksum vectors:** use published fixed CRC vectors with hard-coded expected constants for IMEM and DMEM sequences; include rejected writes and reset/clear. Do not copy model code.
7. **Interrupt/alert temporal suite:** assert exact deferred-delta behavior, W1C masking, enable-after-pending, recoverable 1 ns pulse, sticky fatal alert, and reset cancellation of pending events.
8. **Lifecycle concurrency:** drive escalation and RMA independently and simultaneously during IDLE, EXECUTE, and wipe; assert exact ERR_BITS/fatal cause, memory wipe, OTP request count, ack outputs, done interrupt, and terminal lock.
9. **Algorithm vector suite:** hard-code complete expected buffers for smoke, loop, summation, small RSA cases, and known cryptographic vectors. For negatives assert exact return, ERR_BITS, alert, state, and unchanged/protected output.
10. **RND determinism/bounds:** two fresh DUTs with same seed must produce a hard-coded sequence; different seeds must differ; null output should be handled safely if API contract permits.
11. **Remove coverage-only reachability:** replace direct callback/state tests with legal transactions. If a guard is truly unreachable externally, document/exclude it narrowly rather than manufacturing private access.

## Verdict

OTBN has a broad test inventory; coverage mode is now **fail-closed (2026-09-24, 99.2%)**. Effective assurance remains **medium** because friend-based direct state/callback coverage, synthetic callback execution, non-failing timeouts, and many WARN-only semantic checks are still open. Non-RSA algorithm workers skip RSA-vector tests rather than supplying independent algorithm oracles.
