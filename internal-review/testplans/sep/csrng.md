# CSRNG SystemC/TLM Test Audit and Test Plan

## Scope and evidence basis

Static audit only; runtime coverage and pass/fail status were not measured.

Audited model/build files:

- `include/csrng.h`, `csrng_base.h`, `csrng_register.h`
- `src/csrng.cpp`, `src/csrng_base.cpp`
- `CMakeLists.txt`, `run_tests.sh`, `config/accellera_config.ini`
- Shared inherited transport: `common/include/reg_file.h`

Audited tests:

- `test/inc/testbench.h`, `csrng_basetest.h`, `csrng_test.h`, `csrng_test_enhanced.h`
- `test/src/main.cpp`, `testbench.cpp`, `csrng_test.cpp`, `csrng_basetest.cpp`
- `csrng_func001_drbg_lifecycle.cpp`
- `csrng_func002_pseudorandom_generation.cpp`
- `csrng_func003_seed_life_management.cpp`
- `csrng_func005_command_interface_fsm.cpp`
- `csrng_func008_register_callbacks.cpp`
- `csrng_func009_control_configuration.cpp`
- `csrng_coverage_test.cpp`, `coverage_tests.cpp`

## Model behavior inventory

- Three DRBG instances, each with V, key, compliance, reseed counter and OpenSSL context.
- Software command collection/dispatch for instantiate, generate, reseed, update and uninstantiate, with additional-data buffering.
- CTR-DRBG-like functional generation through OpenSSL random bytes, multi-block GENBITS queue, valid/FIPS flags and sequential reads.
- Entropy request simulation, entropy interrupt, fixed 5 ms local-time increment and FIPS override.
- Seed-life/reseed interval checks, instance validation, multi-bit control validation and access control.
- Internal-state sequential read window and per-instance enable/selection.
- Interrupt status/enable/test/W1C and four output signals.
- Recoverable/fatal alert outputs and error injection/status registers.
- Reset process zeroizing instances, queues, registers, interrupt/alert state and quantum keeper.
- Command FSM thread plus interrupt/alert threads.
- Register access masks/locks including REGWEN and internal-state REGWEN.
- Testbench friendship exposes protected/private helpers and state.
- Inherited register transport supports partial/byte-enable/debug operations, always returns OK, ignores streaming width and provides no DMI.

## Existing scenario-to-function matrix

| Existing scenario group | Model functions/paths reached | Quality |
|---|---|---|
| DRBG instantiate/generate/reseed/update/uninstantiate, deterministic/entropy/additional data (`csrng_func001_drbg_lifecycle.cpp`) | command helpers and output queue | Genuine/Partial |
| Pseudorandom generation and data behavior (`csrng_func002_pseudorandom_generation.cpp`) | GENBITS and output properties | Genuine/Partial; mostly property checks rather than KAT |
| Reseed life, intervals, boundaries and counters (`csrng_func003_seed_life_management.cpp`) | seed-life and command status | Genuine, with duplicated cases |
| Command FSM, sequencing, busy/ready/ack/timing (`csrng_func005_command_interface_fsm.cpp`) | command collection/FSM/status | Genuine/Partial |
| Register resets, masks, locks, RO/WO/RW0C/RW1C and error injection (`csrng_func008_register_callbacks.cpp`) | callback layer | Genuine |
| CTRL encodings, access controls, FIPS force, state-read enable, reset (`csrng_func009_control_configuration.cpp`) | control/config callbacks | Genuine/Partial |
| Invalid instances, direct command helpers, forced FSM states and repeated GENBITS (`csrng_coverage_test.cpp`, `coverage_tests.cpp`) | private helpers/branches | Coverage-only |

## Artificial coverage shortcuts and weaknesses

1. **Critical — the model explicitly friends the testbench for backdoor access.** `include/csrng.h:38-39` and `:86` expose internal command helpers, FSM state, events, buffers and DRBG state. Coverage tests use that access rather than the TLM command/register interface.
2. **Critical — repeated-GENBITS coverage accepts every outcome.** `test/src/csrng_coverage_test.cpp:109-130` directly writes validity, FIPS, read index, buffer and prior 64-bit value. The test then contains an empty conditional that accepts any recoverable-status value and unconditionally passes.
3. **High — coverage forces FSM state and events directly.** `csrng_coverage_test.cpp:93-107` and `coverage_tests.cpp:203-222` assign `m_cmd_fsm_state` and notify internal events. The latter only asserts `MAIN_SM_STATE != 0`, which does not prove each state encoding or transition.
4. **High — command/DRBG helper coverage bypasses software protocol.** `coverage_tests.cpp:40-109` and `csrng_coverage_test.cpp:12-79` directly call `reseed_required`, counter reset, generation and every command method. These checks cannot validate header decode, additional-word collection, ready/busy, command status, timing, interrupts or register access.
5. **High — duplicate coverage files repeat the same shortcuts.** Both files directly test invalid instances and FSM states, inflating execution with little independent semantic value.
6. **Medium — direct state-window loop is line coverage.** `csrng_coverage_test.cpp:160-188` performs 16 reads and extra status reads, then passes without checking the 14-word sequence, wrap, lock denials or unchanged values.
7. **Medium — functional source uses nondeterministic RAND output without a cryptographic KAT at the register boundary.** Tests can establish nonzero/difference/FIPS flags, but not algorithmic CTR-DRBG state evolution.
8. **Medium — TLM helpers pre-seed delay with 10 ns and wait it after transport.** `test/src/csrng_test.cpp:27-39`, `:50-62`, `:73-85`, `:96-108` do not assert DUT-added delay and can conceal an absent or incorrect annotation.
9. **Medium — default CMake globs every coverage source into all builds.** `CMakeLists.txt:143-144`; coverage-only internals run as normal tests if invoked by the harness.
10. **Low — ASan runner lacks explicit ASAN_OPTIONS/log/leak gating.** `run_tests.sh:66-73`.

No `#define private public` is used; friendship provides equivalent backdoor reach.

## Missing or inadequately proven scenarios

- End-to-end tests for invalid instance/helper branches through a real software or hardware command interface rather than direct calls.
- Hardware application command/genbits ports are described by comments but are absent as actual TLM/SystemC interfaces; instances 1–2 cannot be driven end-to-end.
- Real entropy-source interface, request/ack/failure/timeout and FIPS propagation. The model generates entropy internally with `RAND_bytes`.
- Deterministic independent KAT for instantiate/update/generate/reseed state evolution.
- Register target unsupported command, null pointer, zero length, OOB, cross-register, unaligned, partial and byte-enable accesses.
- Debug transport and DMI behavior; streaming width handling.
- Exact transaction or command annotated delays and quantum synchronization, especially the 5 ms entropy delay.
- Reset during command collection, entropy delay, multi-block generation and queued output consumption.
- Concurrent command write while busy and interleaved additional-data sequences.
- Queue exhaustion/multi-block ordering and GENBITS valid transition under partial/out-of-order reads.
- Every repetition-check result with a real generated/injected stream and exact alert bit/output pulse.
- RAND/OpenSSL failure paths.
- Interrupt status-to-output delta timing, simultaneous sources, mask changes on pending state and reset during asserted output.
- Alert pulse observability; `update_alert_outputs()` writes high then low in one method/delta, which may never expose high to a signal observer.
- Error-state persistence/recovery and command rejection after fatal injection.

## Proposed testcases

| ID | Setup / stimulus | Required assertions | Paths | Feature |
|---|---|---|---|---|
| CSRNG-TLM-001 | IGNORE/null/zero/OOB/cross-register payloads | Exact response and no mutation/event | inherited Memory | Command/response/address/length |
| CSRNG-TLM-002 | Byte enables/partial/unaligned writes to CTRL, W1C/RW0C, CMD_REQ | Exact lane merge and side effects only for enabled bytes | callbacks | Byte enable/streaming |
| CSRNG-TLM-003 | Debug accesses to CMD_REQ/GENBITS/status and DMI request | Explicit debug side-effect policy and byte count; DMI false | debug path | Debug/DMI |
| CSRNG-CMD-001 | Send invalid and incomplete command headers only through CMD_REQ | Exact CMD_RDY/ACK/STS, alert/status and no direct helper access | command collection/FSM | TLM/FSM |
| CSRNG-CMD-002 | Interleave writes while busy and reset mid-collection/mid-command | Busy write policy, buffer cancellation and no stale completion | command FSM/reset | Concurrency/reset |
| CSRNG-KAT-001 | Deterministic instantiate/generate/update/reseed sequence with known seed | Exact V/key/output/reseed counters against independent reference | DRBG helpers | Crypto semantics |
| CSRNG-HW-001 | Add/mock real HW client 1/2 request/response paths | Independent instance state, arbitration, exceptions and outputs | missing HW interface | Ports/concurrency |
| CSRNG-ENT-001 | Mock entropy success, non-FIPS, failure and timeout | Request interrupt, exact delay, compliance and command status | entropy request | External interface/time |
| CSRNG-GEN-001 | Generate N blocks; read words in exact and partial sequences | Queue order, valid lifetime, no skipped/repeated block | GENBITS callbacks/queue | Register temporal behavior |
| CSRNG-REP-001 | Deliver two equal 64-bit pairs through normal output flow | Exact comparison-alert bit and observable alert pulse | repetition check | Alerts |
| CSRNG-INTR-001 | Set all sources, mask/unmask and W1C independently | Status sticky, outputs gated, exact delta and reset | interrupt thread | Events/ports |
| CSRNG-ALR-001 | Trigger recoverable/fatal alerts and sample delta cycles | A high pulse is observable for documented duration before low | alert process | SystemC signal timing |

## Coverage quality verdict

**Verdict: Not trustworthy for edge-path coverage.** The functional suites are large and include many real MMIO checks, but both explicit coverage files use friendship to manipulate internals, directly call helpers, force events, loop without value oracles, and in one case accept every alert outcome. This is exactly the kind of execution-only coverage the audit was asked to identify. CMake uses the shared coverage target, but no runtime coverage percentage was proven.
