# HMAC model/test audit

## Scope and evidence

Audited model files: `include/hmac.h`, `include/hmac_base.h`,
`include/hmac_interface.h`, `include/hmac_register.h`,
`include/sha2_engine.h`, `src/hmac.cpp`, `src/hmac_base.cpp`, and
`src/sha2_engine.cpp`.

Audited all files under `test/inc` and `test/src`, plus `CMakeLists.txt`,
`run_tests.sh`, and `doc/test_plan.adoc`.

No measured coverage percentage is claimed. The generic coverage target/gate
is present (`CMakeLists.txt:174-181`, `run_tests.sh:69-72`), but no generated
report was used.

## Behavior inventory

- Regmodel MMIO for interrupt/alert/config/command/status/error, software key,
  digest, message length, wipe, and a 4 KiB message-FIFO window.
- SHA-256/384/512 engine, hash and HMAC modes, known digest formatting,
  endian/key/digest swaps, partial-write packing, message length, and timing.
- START/PROCESS/STOP/CONTINUE sequencing with context export/import.
- Software-key and key-manager dual-share sideload selection.
- FIFO occupancy/full/empty signaling and asynchronous processing threads.
- W1C interrupts, INTR_TEST, alert test, error priority/persistence, secret
  wipe, reset, and interrupt/alert ports.
- Key-manager target socket accepts sideload writes.
- Blocking MMIO comes from `regmodel::Memory`; no explicit nonblocking or DMI
  path is implemented.

## Existing scenario matrix

| Area | Existing evidence | Class | Rationale |
|---|---|---|---|
| SHA-2 known-answer tests | SHA-256, multiblock, SHA-384, SHA-512, empty | Genuine where exact digest arrays are compared | Independent constants exercise the actual MMIO/FIFO/command path. |
| HMAC known-answer tests | Five key/digest combinations | Genuine | Tags are compared and failures increment the suite count. |
| Endian/digest swap | `test_sha256_endian_swap`, `test_sha256_digest_swap` | Coverage-only | The digest words are printed but never compared (`testbench.cpp:436-444`, `533-543`). |
| Interrupts | done/mask/inject/error/fifo-empty | Mixed | Injection has assertions; multiple done/mask/fifo-empty failures are downgraded to INFO. |
| Register access/protection | basic tests, reserved fields, CFG/DIGEST/KEY/WIPE | Partial | Many exact assertions exist, but some reset checks accept mismatches or stop without recording failure. |
| FIFO/packer/length | depth, backpressure, byte/halfword, window, tracking | Mostly Genuine | State and lengths are checked; some interrupt outcomes are permissive. |
| Reset | reset tests and reset-during-processing | Partial | Pin reset is used, but two tests also invoke `dut->reset_all_registers()` directly. |
| Sideload | share writes, XOR, precedence, reset | Genuine for happy path | Engine output is checked; malformed write protocol is not. |
| Reject/context paths | `coverage_tests.cpp` | Genuine/Partial | `test_coverage_gap_paths` has assertions; `test_coverage_reject_paths` only executes branches. |
| TLM protocol | fixed 32/16/8-bit MMIO and fixed keymgr writes | Partial | Valid payloads only; helper response errors do not fail. |

## Shortcut and integrity findings

1. **Critical — several failures call `sc_stop()` without incrementing either
   failure counter.** Examples are `test/src/basic_tests.cpp:389-445`,
   `test/src/basic_tests.cpp:518-537`, and `testbench.cpp:5243-5247`,
   `5357-5361`, `5745-5749`, `6220-6229`. `sc_main` exits nonzero only from
   `m_tests_failed + m_assert_failures`
   (`test/src/testbench.cpp:6256-6267`). A mismatch on one of these paths can
   stop the run early and still exit success.

2. **High — required interrupt outcomes are explicitly permissive.**
   `test/src/testbench.cpp:598-639` treats missing assertion, failed W1C, and
   failed deassertion as INFO. `testbench.cpp:695-747` similarly accepts
   assertion despite masking, failure to assert after enabling, failed W1C,
   and failed deassertion. `testbench.cpp:2591-2606` accepts a missing
   FIFO-empty interrupt.

3. **High — core swap tests have no functional oracle.**
   `test/src/testbench.cpp:436-444` and `533-543` only log digest words, then
   complete. A no-op endian/digest swap passes.

4. **High — one named coverage test is execution-only.**
   `test/src/testbench.cpp:7068-7156` drives reject, context, and sideload-read
   branches but ends with an unconditional PASS log and contains no assertions
   on error codes, state, context, or response status. This is line execution,
   not a test.

5. **High — transport helper errors do not fail tests.**
   `test/src/hmac_test.cpp:7-98` and `102-140` only log bad responses; callers
   cannot inspect status. Thus a failed transaction may leave an old read value
   and continue.

6. **High — key-manager target transport is unchecked.**
   `src/hmac.cpp:1799-1817` dereferences `data_ptr` without checking pointer,
   data length, byte enables, streaming width, or alignment; unknown aligned or
   unaligned addresses still return OK. Tests cover only READ command rejection
   (`test/src/coverage_tests.cpp:211-224`), not malformed writes.

7. **Medium — reset tests directly invoke register reset.**
   `test/src/basic_tests.cpp:370-378` and `559-567` assert `rst_ni` and then
   call `dut->reset_all_registers()`. This can hide a broken reset process or
   incomplete internal-state reset.

8. **Medium — reset mismatches are sometimes accepted.**
   `test/src/basic_tests.cpp:479-487` says a wrong CFG power-on value is
   acceptable; `580-584` only logs a wrong runtime CFG value.

9. **Medium — wait helpers have no timeout.**
   `test/src/testbench.cpp:6271-6290` loops until IDLE/DONE forever. A deadlock
   hangs CI instead of producing a bounded temporal failure.

10. **No `#define private public` was found.** Direct protected/public base
    reset access still bypasses the modeled reset path.

## Missing scenarios

- Exact expected digest for endian_swap, digest_swap, key_swap combinations,
  including toggling digest_swap after completion.
- Strict interrupt temporal checks: assertion deadline, masking, enable-after-
  pending, W1C deassertion, FIFO full→empty qualification, and no spurious
  pulses on reset.
- Alert-test persistence and reset, plus interaction with interrupt/error state.
- Reset at each asynchronous phase: packer partial, full FIFO/backpressure wait,
  block-processing wait, digest-computation wait, STOP/CONTINUE, and active
  sideload.
- STOP off-boundary versus boundary for all digest modes, saved-context
  corruption, missing digest words, upper message length, and continuation with
  changed configuration.
- HMAC key lengths at each exact boundary, sideload shorter/longer than mode,
  invalidated key during an operation, and write failures from the destination.
- SHA-2 engine direct vectors around padding boundaries 55/56/63/64 and
  111/112/127/128 bytes and very large absorbed-bit counts.
- MMIO/keymgr malformed command/address/response/length/pointer/byte-enable/
  streaming-width payloads, delay, `transport_dbg`, DMI, and extensions.

## Proposed testcase matrix

| ID | Setup | Stimulus | Expected | Model path | Protocol feature |
|---|---|---|---|---|---|
| HM-F-01 | SHA-256 known message | Toggle endian_swap and digest_swap independently | Exact independent digest words for all four combinations | Packer/read transform | MMIO functional |
| HM-F-02 | Pending done/error/fifo-empty with enables varied | Enable/mask/W1C at controlled deltas | Exact port and INTR_STATE timing; any mismatch fails | Interrupt method | Port/event |
| HM-F-03 | Partial packer and active processing threads | Assert `rst_ni` at each phase | FIFO/context/key/digest/ports reset through pin only | Reset handler/threads | Reset/temporal |
| HM-F-04 | Save valid context for each SHA mode | Corrupt one digest/length/config field, CONTINUE | Defined error/refusal; no stale digest | Import context | Negative boundary |
| HM-F-05 | Messages at padding boundaries | Hash 55/56/63/64 and 111/112/127/128 bytes | Match independent OpenSSL vectors | `sha2_engine::finalize` | Functional boundary |
| HM-F-06 | FIFO fill cycles | Fill, drain, refill, PROCESS | FIFO-empty interrupt exactly once per qualified cycle | FIFO tracking | Thread/interrupt |
| HM-T-01 | Raw MMIO payload | Valid, hole, out-of-range and unaligned access | Exact response and no mutation on reject | Regmodel memory | Address/response |
| HM-T-02 | Raw MMIO payload | Null pointer; varied lengths/BE/streaming width | Reject malformed forms; exact supported partial writes | Regmodel memory | Pointer/length/BE/streaming |
| HM-T-03 | Raw keymgr payload | READ, IGNORE, invalid address, null/short data, BE | COMMAND/ADDRESS/BURST errors as appropriate; no key change | `keymgr_b_transport` | Full payload validation |
| HM-T-04 | Nonzero delay on both sockets | Valid access | Delay contract asserted or explicitly unchanged | Both transports | Delay |
| HM-T-05 | Debug/DMI initiator | Debug reads and DMI request | Explicit support/denial and side-effect policy | Target socket | Debug/DMI |

## Verdict

**Not trustworthy as a gate until false-success paths are removed.** Strong
cryptographic known-answer coverage exists, but missing assertions are
concentrated in reset, interrupt, swap, and coverage-only cases. Most
importantly, some failure branches stop simulation without affecting the exit
code.
