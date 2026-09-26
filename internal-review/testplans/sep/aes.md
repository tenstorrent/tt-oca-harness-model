# AES SystemC/TLM Test Audit and Test Plan

## Scope and evidence basis

Static audit only; no runtime test or coverage result was generated.

Audited model/build files:

- `include/aes.h`, `include/aes_base.h`, `include/aes_register.h`
- `src/aes.cpp`, `src/aes_base.cpp`
- `CMakeLists.txt`, `run_tests.sh`, `config/accellera_config.ini`
- Shared inherited transport: `common/include/reg_file.h`

Audited tests:

- `test/inc/testbench.h`, `test/inc/aes_basetest.h`, `test/inc/aes_test.h`
- `test/src/testbench.cpp`, `aes_test.cpp`, `aes_basetest.cpp`
- `aes_func001_test.cpp` through `aes_func009_test.cpp`
- `aes_gcm_test.cpp`
- `aes_coverage_test.cpp`, `coverage_tests.cpp`

## Model behavior inventory

- Register model for ALERT_TEST, two 256-bit key shares, IV, DATA_IN/OUT, three shadowed controls, RW0C auxiliary lock, TRIGGER, and STATUS.
- AES-128/192/256 encrypt/decrypt in ECB/CBC/CFB/OFB/CTR using OpenSSL; key is XOR of shares.
- GCM INIT/RESTORE/AAD/TEXT/SAVE/TAG phases, GHASH, partial final blocks, counter increment, save/restore and tag production.
- Automatic start after key/IV/data prerequisites; manual start requiring a key but intentionally allowing incomplete IV/data.
- Automatic-mode output backpressure and manual-mode overwrite/OUTPUT_LOST semantics.
- IV chaining/update for CBC/CFB/OFB/CTR and DATA_OUT read tracking.
- Two-write per-field shadow protocol, sanitization, mismatch recoverable alert, read-to-reset sequence, and lock-gated AUX control.
- Fatal-alert/error lockup, lifecycle escalation, randomized register clearing, recoverable alert, reset-only recovery, and output/status processes.
- Triggered key/IV/input clear, output clear and PRNG reseed; automatic reseed rates and key-touch request.
- Key Manager push socket containing two shares and KEY_CTRL valid bit.
- LT operation/clear/reseed delays using a quantum keeper. Clock frequency is a hard-coded `double`, not a CCI parameter or the `clk_i` signal.
- Inherited register transport supports debug and byte-wise accesses, always responds OK, ignores streaming width, and has no DMI implementation.

## Existing scenario-to-function matrix

| Existing scenario group | Model functions/paths reached | Quality |
|---|---|---|
| OpenSSL equivalence for all key sizes, modes and encrypt/decrypt (`aes_func001_test.cpp`) | key reconstruction, cipher selection/execution | Genuine |
| Sideload equivalence, sideload write blocking, busy write protection and invalid key sanitization (`aes_func002_test.cpp`) | KeyMgr path, key callbacks, CTRL sanitizer | Genuine/Partial |
| IV RW, busy protection, mode-specific update, CTR overflow, CBC chaining (`aes_func003_test.cpp`) | IV callbacks and chaining branches | Genuine |
| Auto start, ready/valid/stall, overwrite protection, pipelining/status (`aes_func004_test.cpp`) | auto-start and output handshakes | Genuine |
| Manual start/preconditions/no-backpressure/OUTPUT_LOST (`aes_func005_test.cpp`) | TRIGGER.START and manual completion | Genuine |
| Shadow matching/mismatch/read reset/field commit/sanitization/REGWEN (`aes_func006_test.cpp`) | all shadow callbacks and recoverable alert | Genuine |
| Key-touch setting and clear triggers (`aes_func007_test.cpp`) | AUX and TRIGGER clear paths | Genuine |
| Fatal/recoverable alerts, escalation, reset recovery and clearing (`aes_func008_test.cpp`) | fatal/recoverable/error-state processes | Genuine/Partial |
| AES operation timing (`aes_func009_test.cpp`) | `calculate_cipher_delay`, quantum keeper, finish timing | Genuine, with broad ±50 ns tolerance |
| NIST GCM encrypt/decrypt, phase gating, sanitization, save/restore (`aes_gcm_test.cpp`) | GCM/GHASH datapath and control | Genuine |
| PRNG, missing sideload key, error-state filters, uncommon GCM guards and key sizes (`aes_coverage_test.cpp`, `coverage_tests.cpp`) | edge branches | Coverage-only/Partial |

## Artificial coverage shortcuts and weaknesses

1. **Critical — error-state coverage test performs writes and passes without proving they were rejected.** `test/src/aes_coverage_test.cpp:156-183` verifies only that fatal alert first asserted, writes GCM control, REGWEN and TRIGGER in ERROR, resets, then unconditionally passes. A model accepting all three writes would pass.
2. **High — AES-192/256 GCM “init” coverage has no semantic assertion.** `test/src/aes_coverage_test.cpp:302-324` configures two key sizes, waits, and unconditionally passes. It only drives lines.
3. **High — PRNG reseed/rate coverage does not verify reseed behavior or rate.** `test/src/aes_coverage_test.cpp:58-99` checks only TRIGGER readback. PER_64 and PER_8K operations are executed, but block-counter/reseed effects and timing are never asserted.
4. **High — repeated coverage suites duplicate branches with weak conclusions.** Both `aes_coverage_test.cpp` and `coverage_tests.cpp` target the same PRNG, escalation, sideload, error and GCM guards. Several cases end in unconditional pass after merely reaching a path (`aes_coverage_test.cpp:239-324`, `:328-364`).
5. **High — GCM output-valid guard is line-driving only.** `aes_coverage_test.cpp:348-362` exports SAVE output, writes IV while OUTPUT_VALID remains set, and then passes without checking IV, STATUS, output, or whether auto-start was refused.
6. **Medium — weak output oracle for sideload manual start.** `coverage_tests.cpp:173-200` accepts any nonzero ciphertext. It does not compare against OpenSSL or the known pushed key, so wrong key share order/content can pass.
7. **Medium — busy GCM write assertion is indirect.** `coverage_tests.cpp:220-257` issues a GCM write during ECB execution but only later checks fatal status and REGWEN after separate ERROR-state writes. It never reads the GCM register to prove the busy write was ignored.
8. **Medium — “no fatal” is used as success for GCM initialization.** `coverage_tests.cpp:330-373` proves no output/no fatal but not that H/S or phase state was correctly initialized.
9. **Medium — KeyMgr transport is under-validated.** Coverage checks reject READ, but the handler at `src/aes.cpp:286-308` dereferences `data_ptr` without validating null/length/alignment and accepts every unknown write address as OK. Tests do not expose this.
10. **Low — all coverage files are always compiled and invoked.** `CMakeLists.txt:143` globs every test source, and `test/src/testbench.cpp:444-457` invokes both coverage groups in normal runs. This avoids a coverage-only executable but masks the distinction between semantic regression tests and line-driving tests.
11. **Low — ASan runner lacks log/leak gating.** `run_tests.sh:66-73` treats ASAN like a normal binary run; it does not set/check an ASan log or explicitly enforce leak detection.

No `#define private public` or direct callback invocation was found.

## Missing or inadequately proven scenarios

- Register target malformed transactions: unsupported command, null pointer, zero length, aperture overrun, cross-register and unaligned accesses.
- Byte-enable semantics for every side-effect register; inherited transport creates a partial `write_value`, while AES callbacks ignore `write_mask`, so disabled bytes can incorrectly clear unstated fields.
- Streaming width behavior, debug transport side-effect policy, and DMI rejection.
- KeyMgr socket null/short/long/unaligned payloads, unknown addresses, byte enables, streaming width, KEY_CTRL=0 clearing shares, and response/address errors.
- `b_transport` annotated delay itself is never asserted; operation timing is observed through spawned work, not payload delay.
- `clk_i` has no behavioral effect and is not tested. Runtime clock changes and invalid frequencies are missing.
- Reset during cipher, clear, GCM or reseed spawned activity; stale thread completion after reset.
- Simultaneous trigger bits and trigger while busy/output-stalled; ordering among START, clear and reseed.
- Exact clear timing and proof that all key/IV/input/output registers changed while preserving required status.
- Automatic reseed thresholds at 1, 64 and 8192, deferred key-touch reseed, and RAND failure path.
- GCM zero/partial lengths for encryption and decryption across AAD/TEXT/TAG, invalid transition matrix coverage, multiple SAVE/RESTORE cycles, and independently computed GHASH intermediate state.
- Fatal alert while an operation is already in flight, with proof no result later appears.
- Shadow partial-byte writes, mismatch in each field, and interaction of read-reset with byte enables.
- Manual start with missing IV/data needs a deterministic expected output oracle, not only status.

## Proposed testcases

| ID | Setup / stimulus | Required assertions | Model paths | Feature |
|---|---|---|---|---|
| AES-TLM-001 | Send IGNORE, null, zero-length, OOB, unaligned and cross-register payloads | Exact response; no mutation/alerts/spawn | inherited `Memory`, callbacks | Command/response/address/length |
| AES-TLM-002 | Partial writes with byte-enable masks to CTRL, AUX, GCM, TRIGGER and DATA_IN | Only enabled lanes affect fields; two-write protocol compares intended bytes | shadow/trigger/data callbacks | Byte enable, streaming width |
| AES-TLM-003 | Debug-read/write STATUS, TRIGGER, DATA_OUT and shadow controls; request DMI | Explicit side-effect-free or side-effectful policy and byte count; DMI false | inherited debug path | Debug/DMI |
| AES-KM-001 | Exercise every KeyMgr address with null, 1/2/3/4/8-byte and unaligned payloads | Errors for malformed/unknown writes; exact shares and valid/shred behavior | `keymgr_b_transport`, `load_sideload_key` | Secondary socket |
| AES-ERR-001 | Enter fatal state, snapshot all writable state, attempt each write/trigger | Every state value unchanged; no spawned clear/reseed/cipher; reset alone recovers | all ERROR guards | Error state |
| AES-RST-001 | Reset during ECB, GCM, clear and reseed | Outputs/STATUS/reset registers exact; no stale completion after reset | reset and spawned processes | Reset/concurrency |
| AES-PRNG-001 | Disable key-touch; run 1/63/64/8191/8192 blocks per configured rate | Reseed exactly at threshold, block counter reset, observable busy interval | reseed helpers | Temporal/threshold |
| AES-TRIG-001 | Write multiple TRIGGER bits together in idle, busy and stall | Defined priority; at most intended workers spawn; final STATUS deterministic | `handle_write_TRIGGER` | Event ordering |
| AES-GCM-001 | Independent NIST vectors for partial AAD/text lengths 1, 15, 16 | Ciphertext/tag exact; bytes beyond valid count zero/ignored | GHASH/TEXT/TAG | Register/datapath |
| AES-GCM-002 | Walk every legal and illegal phase edge, including repeated SAVE/RESTORE | Exact committed phase and alert behavior per edge | `resolve_gcm_phase`, GCM shadow | FSM/shadow |
| AES-CLEAR-001 | Fill every sensitive register, trigger each clear and timestamp | Immediate busy, exact minimum delay, target set randomized, unrelated state preserved | clear workers | Threads/time/security |
| AES-TIME-001 | Change `clk_i` during tests and compare operation latency contract | Either documented no-effect or clock-scaled behavior; no hidden hard-coded mismatch | timing helpers | Port/annotated time |

## Coverage quality verdict

**Verdict: Broad functional coverage, but coverage quality is not acceptable as evidence for several edge paths.** Core cipher, mode, shadow, status and GCM tests have independent oracles. The two explicit coverage files include unconditional passes, path-driving without postconditions, duplicated scenarios and non-cryptographic “nonzero” checks. CMake uses the shared coverage target, but no runtime percentage was inspected. The ASan script path also does not provide the repository’s expected log-based cleanliness proof.
