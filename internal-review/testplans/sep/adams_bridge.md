# Adams Bridge SystemC/TLM Test Audit and Test Plan

## Scope and evidence basis

This is a static source-to-test audit. No test or coverage command was run, so this plan makes no runtime coverage claim.

Audited model/build files:

- `sep/peripherals/adams_bridge/include/adams_bridge.h`
- `sep/peripherals/adams_bridge/include/abr_base.h`
- `sep/peripherals/adams_bridge/include/abr_register.h`
- `sep/peripherals/adams_bridge/include/abr_crypto.h`
- `sep/peripherals/adams_bridge/include/abr_pqc_wrap.h`
- `sep/peripherals/adams_bridge/src/adams_bridge.cpp`
- `sep/peripherals/adams_bridge/src/abr_base.cpp`
- `sep/peripherals/adams_bridge/src/abr_crypto.cpp`
- `sep/peripherals/adams_bridge/src/abr_pqc_mldsa.c`
- `sep/peripherals/adams_bridge/src/abr_pqc_mlkem.c`
- `sep/peripherals/adams_bridge/src/abr_randombytes.c`
- `sep/peripherals/adams_bridge/CMakeLists.txt`
- `sep/peripherals/adams_bridge/run_tests.sh`
- Shared inherited transport implementation: `common/include/reg_file.h`

Audited tests:

- `test/inc/abr_testbench.h`
- `test/src/testbench.cpp`
- `test/src/register_tests.cpp`
- `test/src/mldsa_tests.cpp`
- `test/src/mlkem_tests.cpp`
- `test/src/nist_kat_tests.cpp`
- `test/src/interrupt_kv_tests.cpp`
- `test/src/coverage_tests.cpp`

Vendored PQClean implementation files were treated as third-party algorithm code; the hand-written adapters and every test that exercises them were audited.

## Model behavior inventory

- A 64 KiB `regmodel::Memory<32>` register aperture with identity, control/status, large key/signature/ciphertext windows, Key Vault controls, and interrupt registers.
- Independent ML-DSA and ML-KEM `SC_THREAD` engines. Commands move READY low, publish VALID or ERROR after modeled cycles, and remain parked until zeroize.
- ML-DSA keygen, sign, verify, keygen+sign, context, external-mu, streamed-message/strobe, PCR digest selection, result packing, and zeroization.
- ML-KEM keygen, encapsulation, decapsulation, keygen+decapsulation, implicit rejection, shared-key export, and zeroization.
- Pluggable crypto backends: PQClean FIPS backend and deterministic SHAKE stand-in.
- LT timing through a quantum keeper, cycle-count CCI parameters, frequency input/fallback, synchronization before result publication, and a per-access KV delay.
- Active-low reset clearing engines, buffers, KV state, shares, interrupts, and registers while preserving hardware-tied identity reset values.
- Sticky notification/error status, per-source/global enables, W1C clears, software triggers, saturating counters, and interrupt output thread.
- Flat KV read/write socket plus four write-only dual-share KM sockets with XOR reconstruction, commit/shred control, alignment/length checks, and response statuses.
- Public integration/test seams: backend replacement, PCR digest injection, and direct KV entry loading.
- Inherited register transport accepts reads/writes and debug transport, supports partial/unaligned accesses and byte enables, ignores streaming width, always returns `TLM_OK_RESPONSE` for the register aperture, and provides no DMI implementation.

## Existing scenario-to-function matrix

| Existing scenario | Model functions/paths reached | Quality |
|---|---|---|
| Identity/reset/access masks/window boundaries and round trips (`register_tests.cpp`) | `abr_base`, register masks, reset values, `zeroize()` | Genuine, though broad window loops check storage rather than each field contract |
| ML-DSA keygen/sign/verify, tamper/wrong-message rejection, fused command, randomizer, external mu, context, stream, PCR, zeroize/reset (`mldsa_tests.cpp`) | CTRL callback, engine thread, all primary ML-DSA helpers, message resolution, packing | Genuine for core crypto; Partial for partial-strobe/stream cases that only require completion |
| ML-KEM keygen/encaps/decaps, implicit rejection, fused command, zeroize and engine independence (`mlkem_tests.cpp`) | ML-KEM CTRL/thread and crypto helpers | Genuine |
| ACVP keygen/sign/verify vectors (`nist_kat_tests.cpp`) | Default FIPS backend through MMIO | Genuine |
| Interrupt gating, pending-unmask, W1C, triggers and counters (`interrupt_kv_tests.cpp`) | Interrupt callbacks/thread and status registers | Genuine |
| Flat KV, KV controls, shared-key export and dual-share KM lanes (`interrupt_kv_tests.cpp`) | KV transports, fetch/export, dword reversal and share commit/shred | Genuine, except direct loader check |
| Clock fallback, zero-cycle parameters, reset-issued commands, malformed KV/KM transactions (`coverage_tests.cpp`) | Timing branches and selected socket rejects | Partial |
| Stub/short backend, stream cap, empty stream, direct SHAKE/PQ wrappers (`coverage_tests.cpp`) | Packing edge branches and backend code | Coverage-only/Partial; much of it bypasses the peripheral contract |

## Artificial coverage shortcuts and weaknesses

1. **High — invalid-command test accepts a timeout as success.** `test/src/mldsa_tests.cpp:285-291` passes when `wait_mldsa_valid()` returns false **or** ERROR is set. A model that silently drops the command and never raises ERROR therefore passes after the poll timeout. Require ERROR=1, VALID=0, READY behavior, error interrupt status, and bounded timing independently.
2. **High — direct internal/API coverage bypasses TLM behavior.** `test/src/coverage_tests.cpp:320-362` calls SHAKE, FIPS backend, SHAKE backend, and C wrappers directly. These checks can cover crypto lines but cannot support coverage claims for register decode, command sequencing, status, timing, or interrupts.
3. **High — backdoor is “verified” with a tautological pass.** `test/src/interrupt_kv_tests.cpp:253-256` calls `load_kv_entry()` out of range and then executes `tb.check(true, ...)`. It proves only that the process did not visibly terminate; it does not prove no KV entry changed, no status changed, or that a warning occurred.
4. **Medium — coverage stubs validate their own constants.** `test/src/coverage_tests.cpp:38-137` supplies backends that ignore inputs and return fixed bytes, while `:243-266` checks those same fixed bytes. This is useful dependency injection coverage, but it cannot establish that KV inputs, fused-command inputs, or message modifiers reached the backend correctly.
5. **Medium — short-output cases mostly assert completion only.** `test/src/coverage_tests.cpp:302-318` checks one packed key word, then merely checks completion for sign, verify, ML-KEM keygen/encaps/decaps. Incorrect zero-fill or stale-tail behavior in most output windows would pass.
6. **Medium — broad stream-cap loop checks no cap boundary semantics.** `test/src/coverage_tests.cpp:291-300` performs 16,385 MMIO writes and only asserts that SIGN completes. It does not compare a signature at exactly 65,536 bytes against one after the dropped write, so the cap/drop policy is not proven.
7. **Medium — weak stream/strobe checks.** `test/src/mldsa_tests.cpp:226-252` and `coverage_tests.cpp:268-289` mostly assert command completion/nonzero output. They do not independently calculate expected `mu`, compare equivalent byte streams, or prove disabled strobe lanes are excluded.
8. **Medium — IGNORE command behavior is codified without checking invariance.** `test/src/coverage_tests.cpp:213-217` expects `TLM_OK_RESPONSE` for KV `TLM_IGNORE_COMMAND` but does not verify the destination bytes remain unchanged.
9. **Low — all coverage-only tests run in every build.** `CMakeLists.txt:217-225` unconditionally includes `coverage_tests.cpp`, and `testbench.cpp:329-336` unconditionally invokes it. This avoids a coverage-only binary split, but mixes backend unit tests and high-cost line-driving loops into the functional suite.

No `#define private public` was found. Direct access occurs through deliberately public integration hooks and direct backend objects.

## Missing or inadequately proven scenarios

- Register target TLM protocol: `TLM_IGNORE_COMMAND`, null data, zero length, out-of-range/cross-aperture addresses, unaligned accesses, partial lengths, byte-enable patterns/length, streaming width smaller than data length, response status, and no mutation on rejected/ignored operations.
- `transport_dbg` read/write behavior and proof that debug accesses have the intended side effects; DMI request behavior.
- KV socket: zero-length transfer, exact final-byte boundary, cross-entry transfer, read/write with nontrivial byte enables/streaming width, unsupported command invariance, and annotated delay value.
- KM sockets: `len > 4`, cross-word data, byte enables, zero length, every invalid lane/address boundary, valid=false clearing all share scratch, and delay value.
- Exact command latency for each operation and zeroize; fallback frequency and parameter override are exercised but elapsed simulated time is not asserted.
- Real overlap: start ML-DSA and ML-KEM before either completes, assert independent busy/valid timing, and reset one shared block while both are pending. Current “independence” test is sequential.
- Commands while already busy/parked and simultaneous zeroize/command writes.
- Reset during an in-flight operation, including stale spawned completion after reset and interrupt/result suppression.
- Interrupt counter saturation at `0xffffffff`, counter increment-pulse clearing, both errors and notifications pending simultaneously, and reset while outputs are asserted.
- Full zeroization of every sensitive input/output/internal stream/KV/share path; current tests sample selected windows.
- KV dword reversal against a non-palindromic independently calculated expected seed; current test only proves divergence from direct order.
- PCR digest exact message selection and clearing across reset/zeroize.
- Context lengths 0, 1, 255 and reserved/high bits; stream strobe patterns other than 0, 3 and F.
- Backend exception/failure behavior and malformed buffer assumptions.
- CCI cycle parameters at negative values, invalid fallback clock, and dynamic parameter changes during operation.

## Proposed testcases

| ID | Setup / stimulus | Required assertions | Model functions / paths | TLM/SystemC feature |
|---|---|---|---|---|
| ABR-TLM-001 | Issue IGNORE, null-data, zero-length, out-of-range and cross-boundary register/KV/KM payloads | Exact response per command; no register/KV mutation; no callback side effect | `Memory::b_transport`, `keymgr_b_transport`, `km_share_b_transport` | Command/response/address/data length |
| ABR-TLM-002 | Perform unaligned 1/2/3/5-byte accesses with byte-enable masks and narrow streaming width | Independently expected byte merge/read bytes; untouched disabled lanes | Register windows and socket handlers | Byte enable, streaming width, unaligned access |
| ABR-TLM-003 | Use `transport_dbg` on RW, WO, side-effect and reserved offsets; request DMI | Returned byte count/data and explicit side-effect policy; DMI false | Inherited register memory | Debug transport, DMI |
| ABR-TIME-001 | Set known frequency/cycle params; timestamp each command and zeroize | Busy visible immediately; VALID/READY changes only after exact modeled interval within one delta | `consume_cycles`, finish helpers, both threads | Annotated/SystemC time |
| ABR-TIME-002 | Start ML-DSA and ML-KEM in same delta; reset before completion | Independent completions without serialization; reset cancels stale outputs/interrupts | Both engine threads, reset handler | Concurrency, events, reset |
| ABR-ERR-001 | Send each invalid CTRL encoding | ERROR=1, VALID=0, READY documented value, error status/counter/IRQ exact; never accept timeout alone | CTRL callbacks, `raise_error` | Error path and interrupt |
| ABR-KV-001 | Cross KV entry boundary and final aperture byte; then fetch and run operation | Exact bytes in both entries and exact backend-observed seed/message | KV transport/fetch | Address boundary, data length, delay |
| ABR-KM-001 | Commit known non-palindromic shares; independently reverse dwords | Exact reconstructed KV bytes and exact derived public key against independent backend call | KM share transport, `reverse_le_dwords` | Secondary target socket |
| ABR-STREAM-001 | Sign equivalent messages via fixed MSG and multiple strobe/chunk sequences | Equal signatures for equal byte streams; changed signature for one enabled byte; ignored disabled lanes | `mldsa_msg_write`, `resolve_sign_message` | Register side effects |
| ABR-STREAM-002 | Sign at 65,532, 65,536 and 65,540 attempted bytes | First two differ as expected; post-cap write produces exactly same result as capped stream | Stream cap branch | Boundary/large temporal stimulus |
| ABR-ZERO-001 | Fill every sensitive window, KV entry, share lane, stream and PCR digest; zeroize/reset | Every externally observable item cleared; subsequent operation cannot reuse stale material | `zeroize`, `reset_handler` | Reset/security state |
| ABR-INTR-001 | Seed counters near max, trigger repeatedly, mask/unmask and clear both sources | Saturation, sticky status, aggregate/global gating and output delta timing | Interrupt helpers/thread | Events/ports/W1C |

## Coverage quality verdict

**Verdict: Partial, with high-confidence core crypto coverage but material coverage-quality debt.** The ACVP and round-trip suites are meaningful and most MMIO functional paths are assertion-backed. However, direct backend calls, constant-output stubs, a tautological backdoor check, a timeout-accepting invalid-command assertion, and completion-only edge tests inflate executed lines without proving several semantics. Coverage CMake wiring includes the full suite and uses the shared gate, but no measured percentage was inspected or proven in this audit.
