# EDN SystemC/TLM Test Audit and Test Plan

## Scope and evidence basis

Static source audit only. No runtime or coverage result is claimed.

Audited model/build files:

- `include/edn.h`, `edn_base.h`, `edn_register.h`, `edn_csrng_interface.h`
- `src/edn.cpp`, `src/edn_base.cpp`
- `CMakeLists.txt`, `run_tests.sh`, `config/accellera_config.ini`
- Shared inherited transport: `common/include/reg_file.h`

Audited test files:

- `test/inc/edn_test.h`, `edn_basetest.h`, `testbench.h`
- All `test/inc/test_edn_func_001.h` through `test_edn_func_014.h`
- `test/src/edn_test.cpp`, `edn_basetest.cpp`, `testbench.cpp`
- All `test/src/test_edn_func_001.cpp` through `test_edn_func_014.cpp`

## Model behavior inventory

- Seventeen-register EDN block: interrupt/alert/control/status, boot commands, SW command FIFO/status, HW status, reseed/generate FIFOs, max requests, recoverable/fatal status and main FSM state.
- Multi-bit CTRL validation, REGWEN lock, boot/auto/software mode transitions and sticky Error state.
- SW command collection with header-length parsing and simulated CSRNG acknowledgment.
- Boot instantiate/generate/uninstantiate and auto-mode dispatcher/generate/reseed behavior.
- Reseed and generate FIFOs with 13-word depth and fatal overflow handling.
- Recoverable CSRNG-ack and entropy-bus comparison alerts; fatal error injection and FIFO errors.
- Interrupt status/enable/test/W1C and two output signals.
- Entropy reception into an internal queue and FIPS tracking.
- Reset clears registers, buffers/FIFOs, consistency state and dispatcher epochs.
- LT local-time accounting in auto mode; `clk_i` exists but is not materially used for timing.
- No real CSRNG command/genbits TLM socket or SystemC handshake is present. Ack is a public forced status; entropy is injected by a friend.
- No EDN endpoint request/ack/data/FIPS ports exist in `edn_ip`. Endpoint behavior in tests is implemented in `edn_test`.
- Inherited register transport supports partial/BE/debug, always returns OK, ignores streaming width and has no DMI.

## Existing scenario-to-function matrix

| Existing suite | Scenarios/functions | Quality |
|---|---|---|
| FUNC001 | reset/access types, commands/status, register basics | Genuine/Partial |
| FUNC002/FUNC006 | CTRL encodings, REGWEN, reset, state entry | Genuine/Partial |
| FUNC003 | MAIN_SM_STATE visibility/transitions/error state | Partial; accepts broad state sets |
| FUNC004/FUNC014 | SW command accumulation/status/sequencing | Genuine for local simulation; Partial for CSRNG behavior |
| FUNC005/FUNC011 | endpoint request, fairness, data/FIPS distribution | Coverage-only for `edn_ip`; endpoint service is testbench code |
| FUNC007 | reseed/generate FIFO depth/overflow/reset | Genuine |
| FUNC008 | entropy bus comparison | Partial; entropy enters through direct friend call |
| FUNC009 | interrupt status/enable/test/outputs | Genuine |
| FUNC010 | alert/error injection/status/clearing | Genuine/Partial |
| FUNC012 | boot mode state/status/exit | Partial; accepts multiple asynchronous outcomes |
| FUNC013 | auto mode and reseed cadence | Partial; simulated internal CSRNG |

## Artificial coverage shortcuts and weaknesses

1. **Critical — endpoint tests test the harness, not the DUT.** `test/src/edn_test.cpp:201-237` contains `mock_endpoint_process()` that directly drives `edn_ack`, `edn_bus` and `edn_fips`; these are test-harness signals, not `edn_ip` ports. Endpoint suites can pass even if the model has no endpoint implementation—which it does not.
2. **Critical — direct friendship/backdoor replaces CSRNG integration.** `include/edn.h:84-86` friends `edn_test`; public `force_csrng_ack_status()` is at `:128-129`. `edn_test.cpp:249-260` directly calls that hook and private `receive_csrng_entropy()`. This bypasses a CSRNG interface, timing, protocol, arbitration and error response.
3. **Critical — coverage target runs far more tests than Release/CTest/ASan.** `CMakeLists.txt:173-181` runs suite IDs 1 through 14 only for coverage. `run_tests.sh:66-73` and the CTest registration run the binary without a suite ID, selecting the default suite 4. Thus most assertions are coverage-only and are not exercised by the ordinary or sanitizer run.
4. **High — hand-written model code is excluded from coverage.** `src/edn.cpp:287-308`, `:841-901`, `:1405-1423`, and `:1494-1503` use `LCOV_EXCL_*` around REGWEN, HW command, alert and invalid error-injection paths. These are hand-written, reachable semantics and should not be removed from the denominator.
5. **High — legacy tests explicitly skip implemented functionality.** `test/src/testbench.cpp:333-339` reports REGWEN protection skipped even though the callback exists, creating misleading suite summaries.
6. **High — initialization test accepts nearly any non-idle state.** `test_edn_func_002.cpp:1097-1104` defines operational as `SWPortMode || state != Idle`, so boot, auto, error or unknown non-idle states satisfy it.
7. **High — Generate-after-Uninstantiate mismatch is only a warning.** `test_edn_func_014.cpp:591-600` says generate should fail/reinstantiate, but success does not fail the testcase.
8. **Medium — MAIN_SM transition warnings do not fail.** `test_edn_func_003.cpp:227-253` warns if state remains Idle and then accepts any known sparse encoding, rather than requiring SWPortMode.
9. **Medium — boot test accepts multiple states/command types.** `test_edn_func_012.cpp:164-192` accepts either instantiate or generate state/type after a fixed wait. This may be reasonable for asynchronous progression but does not prove ordered transitions or timing.
10. **Medium — SW endpoint test deliberately skips instantiate.** `test_edn_func_011.cpp:274-279` comments out command setup and injects entropy directly, so it cannot prove software-mode command-to-data flow.
11. **Low — port binding checks are unconditional.** `test/src/testbench.cpp:347-367` increments pass counters for TLM/interrupt/alert binding without driving or observing each connection.
12. **Low — ASan runner has no explicit log/leak gate.** `run_tests.sh:66-73`.

No `#define private public` was found; `friend class edn_test` provides equivalent privileged access.

## Missing or inadequately proven scenarios

- Actual endpoint ports and DUT-owned arbitration/distribution behavior. Current fairness, persistence and FIPS tests are tests of `mock_endpoint_process`.
- Actual CSRNG command/genbits interface with request/ready/ack/status/data/FIPS timing, errors, backpressure and reset.
- End-to-end boot, auto and SW flows through connected CSRNG and endpoint consumers.
- Release and ASan execution of all 14 suites.
- Every currently excluded LCOV path through public/TLM stimulus.
- TLM unsupported command, null pointer, zero length, OOB/cross-map, partial, unaligned and byte-enable behavior.
- `transport_dbg`, DMI and streaming-width behavior.
- Payload annotated delay and use of `clk_i`; dynamic/zero clock behavior.
- Reset while SW multiword command, boot sequence, auto dispatcher or CSRNG response is pending.
- Concurrent endpoint requests while entropy arrives/exhausts, fairness over many rounds and deassertion ordering—once implemented in DUT.
- Strict state-transition ordering instead of accepting multiple broad outcomes.
- Generate after uninstantiate, commands while disabled/wrong mode, FIFO writes in Error, and reset-only recovery.
- RAND failure, huge boot `glen`, auto epoch cancellation and stale spawned-thread suppression.
- Alert pulse/level semantics and simultaneous recoverable/fatal conditions.

## Proposed testcases

| ID | Setup / stimulus | Required assertions | Paths | Feature |
|---|---|---|---|---|
| EDN-WIRE-001 | Add/bind real CSRNG mock interface and execute SW instantiate/generate | Exact command words, ready/ack/status ordering, data/FIPS delivery | SW command and CSRNG path | TLM/SystemC interface |
| EDN-WIRE-002 | Bind eight DUT endpoint ports; request simultaneously | DUT-owned one-grant arbitration, round-robin fairness, data/FIPS stability until req drops | endpoint logic | Ports/concurrency |
| EDN-MODE-001 | Step boot mode one handshake at a time | Exact ordered FSM states and HW status, then clean uninstantiate exit | boot threads | FSM/temporal |
| EDN-MODE-002 | Configure auto mode with small reseed interval and controlled CSRNG replies | Exact generate/reseed cadence, counter and Error behavior | auto dispatcher | Threads/events |
| EDN-CMD-001 | Instantiate, uninstantiate, then generate | Generate rejected with nonzero status; no entropy; state remains defined | SW command path | Error sequence |
| EDN-RST-001 | Reset during partial SW command and each spawned boot/auto phase | Buffers cleared, epochs cancel stale workers, no post-reset status/interrupt | reset/threads | Reset/concurrency |
| EDN-TLM-001 | IGNORE/null/zero/OOB/cross-register payloads | Exact errors/no mutation | inherited Memory | Command/address/length |
| EDN-TLM-002 | Byte-enabled/unaligned writes to CTRL, W1C/W0C and command FIFOs | Only enabled bytes affect fields/side effects | callbacks | Byte enable/streaming |
| EDN-TLM-003 | Debug accesses and DMI request | Explicit side-effect policy/byte count; DMI false | debug path | Debug/DMI |
| EDN-FIFO-001 | 13 and 14 words through TLM in all modes/error state | Exact accept/refuse, sticky bits, interrupt and alert | FIFO callbacks | Boundary/error |
| EDN-ALR-001 | Inject duplicate entropy through real CSRNG channel | Exact comparison bit, continued distribution, clear behavior | consistency check | Alert/data |
| EDN-CI-001 | Run all suite IDs in Release, ASan and Coverage | Same scenario set in all modes; no suite silently omitted | CMake/run script | Coverage wiring |

## Coverage quality verdict

**Verdict: Critical coverage-integrity failure.** Most suites run only under the coverage target; endpoint suites exercise testbench-owned behavior absent from the DUT; CSRNG behavior uses friend backdoors; and reachable hand-written model regions are explicitly excluded with LCOV markers. Some register/FIFO/interrupt tests are meaningful, but aggregate line coverage cannot be treated as evidence of EDN model correctness until these structural shortcuts are removed. No runtime percentage was inspected.
