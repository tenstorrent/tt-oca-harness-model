# KMAC model/test audit

## Scope and evidence

Audited model files: `include/kmac.h`, `include/kmac_base.h`,
`include/kmac_interface.h`, `include/kmac_register.h`, `src/kmac.cpp`, and
`src/kmac_base.cpp`.

Audited every file under `test/inc` and `test/src`, plus `CMakeLists.txt`,
`run_tests.sh`, and `doc/test_plan.adoc`.

No new measured coverage result is claimed. The checked-in test plan states
94.4% (`doc/test_plan.adoc:13-17`), but this audit did not independently
measure it. The coverage build deliberately enables verbose logging to avoid
counting suppressed stream-formatting lines (`CMakeLists.txt:88-97`).

## Behavior inventory

- Regmodel MMIO for interrupts, alerts, shadowed configuration, command/status,
  entropy controls, software key shares/length, prefix, error code, STATE, and
  MSG_FIFO windows.
- SHA-3, SHAKE, cSHAKE, and KMAC via OpenSSL, including prefix/domain encoding,
  fixed and XOF output, masking shares, state/message endianness.
- IDLE/ABSORB/SQUEEZE/ERROR/ESCALATION_LOCKED FSM, sparse commands, auxiliary
  command bits, register write protection, errors, alerts and interrupts.
- 64-bit packer, FIFO occupancy, delayed drain/backpressure, fifo-empty
  qualification, and quantum-keeper timing.
- Software keys and key-manager push sideload target socket.
- Three application interfaces, arbitration/lockout, digest shares, and app
  completion/error.
- EDN/SW/idle entropy abstractions, six-word SW seed, refresh counter/threshold,
  reset and lifecycle escalation zeroization.
- Blocking MMIO inherited from `regmodel::Memory`; no explicit nb-transport or
  DMI implementation.

## Existing scenario matrix

| Area | Existing evidence | Class | Rationale |
|---|---|---|---|
| SHA-3/SHAKE/cSHAKE/KMAC KATs | FUNC001-004 | Mostly Genuine | Many digest/MAC outputs are compared against OpenSSL references through MMIO. |
| Software/sideload keys | FUNC005-006 | Mostly Genuine | Keys, lengths, sideload selection and zeroization are exercised; white-box additions weaken some cases. |
| App interfaces | FUNC007-009 | Genuine/Partial | Public app interfaces are driven and outputs checked; some later coverage cases manipulate internals. |
| FIFO/packer | FUNC010 | Genuine for core cases | Depth, granularity, state errors and backpressure are exercised. TC-166/167 are empty duplicate passes. |
| FSM/errors/shadow/STATE/reset/escalation | FUNC011-022/026 | Mostly Genuine | Exact state/error/alert checks exist, with several permissive boundary or white-box exceptions. |
| FUNC023 OpenSSL seam | Compiled source only | Coverage-only/not executed | No call site exists for its test functions. |
| FUNC024 timing suite | Compiled source only | Coverage-only/not executed | No call site exists for its four tests. |
| FUNC025 gap fillers | TC-200+ | Mixed/Coverage-only | Some assert exact errors; many execute branches and unconditionally pass. |
| TLM protocol | Fixed MMIO helpers and keymgr writes | Partial | Helpers log errors only; malformed payload coverage is absent. |

## Shortcut and integrity findings

1. **Critical — coverage code mutates a `const` model configuration object via
   `const_cast`.** `test/src/kmac_func025_test.cpp:470-483`,
   `522-579`, and `596-640` cast away const from `dut->EnMasking` and write it.
   `EnMasking` is declared `const` (`include/kmac.h:115-117`), so modifying it
   is undefined behavior. These are not valid tests of an unmasked
   configuration; they create a state no constructed DUT can legally enter.

2. **High — extensive friend-based white-box branch execution is counted as
   functional testing.** `include/kmac.h:121-125` grants testbench friendship.
   `test/src/kmac_func025_test.cpp:466-516` directly changes registers,
   queues, OpenSSL pointers, and callbacks, invokes private methods with invalid
   indices, then unconditionally reports PASS.

3. **High — multiple named tests contain no assertion and always pass.**
   Examples: TC-166/167 (`kmac_func010_test.cpp:1103-1119`);
   TC-204/205 (`kmac_func025_test.cpp:132-176`);
   TC-212/213 (`kmac_func025_test.cpp:299-331`);
   TC-214-218 (`kmac_func025_test.cpp:335-517`). These only execute lines or
   log outcomes.

4. **High — entire claimed FUNC023 and FUNC024 groups are not orchestrated.**
   Their functions are defined in `kmac_func023_test.cpp` and
   `kmac_func024_test.cpp`, but searches find no call sites. `run_tests()`
   proceeds from FUNC022 to FUNC025 (`test/src/testbench.cpp:685-718`), while
   `doc/test_plan.adoc:61-62` claims FUNC023-024 coverage. Compiling a test
   source is not executing it.

5. **High — transport errors do not fail ordinary tests.**
   MMIO helpers only print to stderr (`test/src/kmac_test.cpp:83-129`,
   `135-182`); keymgr helpers do not check status at all
   (`kmac_test.cpp:184-229`).

6. **High — key-manager target transport accepts malformed writes.**
   `src/kmac.cpp:505-537` checks only that the command is WRITE, then
   dereferences `data_ptr`; it ignores pointer validity, length, alignment
   beyond selected address cases, byte enables, streaming width and delay.
   Unknown addresses still receive OK. TC-230 only issues a read and has no
   response-status assertion (`kmac_func026_test.cpp:462-471`).

7. **High — timing tests are permissive even where present.**
   `kmac_func024_test.cpp:210-237` accepts a FIFO that never becomes full and a
   zero write duration. The function reports PASS even after those conditions.
   Similar “INFO/expected depending on configuration” behavior appears at
   `kmac_func024_test.cpp:408-429`. These functions are additionally not run.

8. **Medium — permissive expected outcomes obscure the intended branch.**
   TC-225 accepts either error 0x06 or 0x08
   (`kmac_func026_test.cpp:296-309`). TC-227 checks only that error 0x80 did
   not occur, not that output clamped to the correct final chunk
   (`kmac_func026_test.cpp:360-385`). TC-228's
   `share0 != 0 || share1 == 0` condition does not prove separate window
   decoding on the actual masked DUT (`kmac_func026_test.cpp:397-418`).

9. **Medium — `run_test()` is a test-specific placeholder that always returns
   true.** `test/src/testbench.cpp:848-859`. It is not used by the main
   orchestrator, but it is unsafe for future callers and constitutes a latent
   false-green hook.

10. **No `#define private public` was found**, but `friend class testbench`
    provides equivalent private access used by the coverage-only cases.

## Missing scenarios

- Constructed `EnMasking=false` DUT test target; no legal unmasked test exists.
- Executed and strict timing assertions for START/PROCESS/DONE, entropy latency,
  full-FIFO backpressure, app completion, incoming annotated delay, and quantum
  synchronization.
- Strict keymgr target protocol: invalid address/alignment, null pointer,
  length, byte enables, streaming width, delay, and response.
- Independent NIST vectors at message/rate/padding boundaries; long
  customization exact-output oracle; exact final XOF chunk/clamp behavior.
- Application arbitration with same-delta requests, partial strobes over
  multiple beats, reset/escalation during an open app message, and completion
  pulse lifetime.
- Interrupt and alert temporal behavior: enable-after-pending, masking, W1C,
  reset, fifo full→drain→refill, and no duplicate event.
- Shadowed writes with byte enables, reset between first/second write, interleave
  of two shadowed registers, and same-delta escalation.
- OpenSSL failure injection for fetch/init/update/final/XOF/MAC allocations.
- MMIO holes/bounds/alignment, unsupported command, null data, varied lengths,
  byte enables, streaming widths, debug transport, DMI, delay and extensions.

## Proposed testcase matrix

| ID | Setup | Stimulus | Expected | Model path | Protocol feature |
|---|---|---|---|---|---|
| KMAC-C-01 | Construct DUT with `EnMasking=false` | Run SHA3/SHAKE/KMAC and app operations | Exact digest in share0; share1 zero; no UB/private mutation | Unmasked branches | Legal configuration |
| KMAC-F-01 | Independent NIST vectors | Messages at rate/padding boundaries and long customization | Exact STATE/XOF/MAC bytes | OpenSSL/prefix/packer | Functional boundary |
| KMAC-F-02 | FIFO fills to capacity | One extra write, wait, drain, refill | Full observable; exact nonzero stall; fifo-empty IRQ once | FIFO drain/backpressure | Temporal/thread/IRQ |
| KMAC-F-03 | Two app ports request in same delta | Partial and final beats with varied strobes | Fixed winner, loser error/no splice, exact digest | App arbitration | Port/event |
| KMAC-F-04 | First shadow write pending | Reset, interleave, mismatch, partial BE, escalate | Exact commit/alert/discard behavior | Shadow callbacks | Reset/BE/event |
| KMAC-F-05 | XOF/KMAC output near exhaustion | RUN to final and one beyond | Exact final chunk, clamp/error, unchanged prior data | RUN window | Boundary |
| KMAC-F-06 | Inject OpenSSL allocation/final failures | START/PROCESS/app request | Exact ERR_CODE/alert/cleanup/no leak | Crypto failure paths | Negative functional |
| KMAC-T-01 | Raw MMIO payload | Valid/hole/out-of-range/unaligned | Exact response; no mutation on reject | Regmodel memory | Command/address/response |
| KMAC-T-02 | Raw MMIO payload | Null pointer; lengths/BE/streaming variants | Exact supported semantics or rejection | Regmodel memory | Pointer/length/BE/streaming |
| KMAC-T-03 | Raw keymgr payload | READ/IGNORE/bad address/null/short/BE | Exact error status; no key state change | `keymgr_b_transport` | Full payload validation |
| KMAC-T-04 | Valid access with nonzero delay | MMIO and keymgr calls | Delay contract asserted | Both sockets/QK | Delay |
| KMAC-T-05 | Debug/DMI initiator | `transport_dbg`, DMI request | Explicit support/denial and side-effect policy | Target socket | Debug/DMI |

## Verdict

**Coverage result is materially contaminated by white-box execution-only
cases.** The main cryptographic and FSM suites contain substantial genuine
coverage, but the undefined `const_cast`, unconditional passes, and uncalled
FUNC023/024 suites make the aggregate test count and coverage-oriented claims
unreliable. Protocol coverage is also shallow on both target sockets.
