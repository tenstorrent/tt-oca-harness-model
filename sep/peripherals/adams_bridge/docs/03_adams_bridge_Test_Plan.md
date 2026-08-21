# Adams Bridge SystemC TLM Model Test Plan

**Document Version:** 1.0
**Date:** 2026-08-20
**IP Module:** Adams Bridge (ABR)
**Target:** Standalone SystemC TLM-2.0 testbench (`adams_bridge_test`)

---

## 1. Test Plan Overview

### 1.1 Objectives

- Prove the RDL access types (RO / WO / RW) and identity reset values.
- Prove the READY/VALID/ZEROIZE handshake firmware uses.
- Prove ML-DSA and ML-KEM command flows, including fused commands,
  modifiers, tamper detection and implicit rejection.
- Prove interrupt masking, W1C, counters and software triggers.
- Prove Key Vault sideload import/export, KM dual-share reconstruct, and failure paths.
- Prove the crypto-backend seam (`set_crypto_backend`) and NIST ACVP KATs
  against the default FIPS backend.

### 1.2 Environment

- `abr_testbench` (`test/inc/abr_testbench.h`): TLM initiator into the
  register aperture, second initiator into `keymgr_tl_socket`, KM share
  initiators into `keymgr_*_socket`, `clk_i` / `rst_ni`, and
  `run_mldsa` / `run_mlkem` / `zeroize_*` helpers that mirror `abr_mldsa.h`.
- Default `abr_fips_backend` (PQClean); `abr_shake_backend` and a unit-test
  stub are installed via `set_crypto_backend()`.
- Build: `./run_tests.sh`, `./run_tests.sh --asan`, `./run_tests.sh --coverage`.

### 1.3 Ports used

| Port | Use |
|---|---|
| `abr_base` TLM target | All MMIO |
| `keymgr_tl_socket` | Flat KV push/read |
| `keymgr_*_socket` | KM dual-share sideload |
| `rst_ni` | `reset_dut()` |
| `intr_abr_error` / `intr_abr_notif` | Interrupt pin checks |
| `clk_i` | Bound to 100 MHz in the bench |

### 1.4 Success criteria

- Every `tb.check` / `tb.check_eq` in the four suites passes.
- ASan/UBSan run produces no `asan.log.*`.
- Line coverage on the touched model sources meets the repo ≥ 95% gate.

---

## 2. Test Case Table

Sources: `test/src/register_tests.cpp`, `mldsa_tests.cpp`,
`mlkem_tests.cpp`, `interrupt_kv_tests.cpp`. Names are the `tb.section()`
titles.

| Sl. | TestCase Name | Description | Registers / ports | Type |
|---:|---|---|---|---|
| **Register access** |
| 1 | Register: identity and reset state | `MLDSA_NAME` = `'MLDSA-87'`, versions non-zero, both STATUS.READY set, VALID/ERROR clear | NAME, VERSION, STATUS | Positive |
| 2 | Register: access-type enforcement | RO NAME/STATUS ignore writes; WO SEED/ENTROPY/PRIVKEY_IN/SEED_D/CTRL read as 0 | NAME, STATUS, SEED, ENTROPY, PRIVKEY_IN, MLKEM_SEED_D, CTRL | Positive |
| 3 | Register: field masking | CTX_CONFIG and MSG_STROBE are WO | CTX_CONFIG, MSG_STROBE | Positive |
| 4 | Register: data windows round-trip | PUBKEY (648), SIGNATURE (1157), ENCAPS (392), CIPHERTEXT (392), DECAPS (792) survive write/read | key/signature windows | Positive |
| 5 | Register: window boundaries | Last dword of PUBKEY/SIGNATURE/PRIVKEY_OUT is decoded; zeroize clears windows | window last words, CTRL.ZEROIZE | Positive |
| **ML-DSA** |
| 6 | ML-DSA: keygen | KEYGEN completes, ERROR clear, READY stays low, pubkey/privkey non-zero | SEED, ENTROPY, CTRL, STATUS, PUBKEY, PRIVKEY_OUT | Positive |
| 7 | ML-DSA: keygen determinism | Same seed reproduces the same public key | SEED, CTRL, PUBKEY | Positive |
| 8 | ML-DSA: sign | SIGN with PRIVKEY_IN produces a non-zero signature | PRIVKEY_IN, MSG, SIGN_RND, SIGNATURE | Positive |
| 9 | ML-DSA: verify accepts a valid signature | VERIFY_RES matches the signature c-tilde prefix | PUBKEY, SIGNATURE, MSG, VERIFY_RES | Positive |
| 10 | ML-DSA: verify rejects a tampered signature | VERIFY_RES diverges after flipping a signature word | SIGNATURE, VERIFY_RES | Negative |
| 11 | ML-DSA: verify rejects a wrong message | Different MSG fails the c-tilde compare | MSG, VERIFY_RES | Negative |
| 12 | ML-DSA: KEYGEN+SIGN fused command | CTRL=4 produces both keys and a signature | CTRL, PUBKEY, SIGNATURE | Positive |
| 13 | ML-DSA: signature randomizer | Different SIGN_RND yields a different signature | SIGN_RND, SIGNATURE | Positive |
| 14 | ML-DSA: EXTERNAL_MU | CTRL.EXTERNAL_MU uses MLDSA_EXTERNAL_MU | EXTERNAL_MU, CTRL bit 5 | Positive |
| 15 | ML-DSA: signing context | CTX_SIZE + CTX changes the signature vs empty context | CTX, CTX_CONFIG | Positive |
| 16 | ML-DSA: streamed message | MSG writes before CTRL.STREAM_MSG are hashed | MSG, CTRL bit 6 | Positive |
| 17 | ML-DSA: partial-word strobe | MSG_STROBE masks bytes of a streamed word | MSG_STROBE, MSG | Positive |
| 18 | ML-DSA: PCR_SIGN modifier | SIGN with PCR_SIGN completes; empty digest matches MSG; `set_pcr_digest` diverges | CTRL bit 4 | Positive |
| 19 | ML-DSA: error and no-op paths | CTRL=5 raises ERROR; CTRL=0 leaves READY and does not set VALID | CTRL, STATUS | Negative |
| 20 | ML-DSA: zeroize clears key material | PUBKEY / PRIVKEY_OUT / VERIFY_RES become zero; READY returns | ZEROIZE | Positive |
| 21 | ML-DSA: reset | `rst_ni` restores READY and clears windows | rst_ni | Positive |
| **ML-KEM** |
| 22 | ML-KEM: keygen | KEYGEN, READY stays low, encaps/decaps keys non-zero | SEED_D/Z, ENCAPS_KEY, DECAPS_KEY | Positive |
| 23 | ML-KEM: keygen determinism | Same seeds reproduce the encaps key | SEED_D/Z | Positive |
| 24 | ML-KEM: encapsulation | ENCAPS produces ciphertext and shared key | ENCAPS_KEY, MSG, CIPHERTEXT, SHARED_KEY | Positive |
| 25 | ML-KEM: decapsulation agrees with encapsulation | DECAPS shared key matches ENCAPS | DECAPS_KEY, CIPHERTEXT, SHARED_KEY | Positive |
| 26 | ML-KEM: implicit rejection | Tampered ciphertext yields a different shared key, ERROR stays clear | CIPHERTEXT, SHARED_KEY, STATUS.ERROR | Negative |
| 27 | ML-KEM: KEYGEN+DECAPS fused command | CTRL=4 produces keys and a shared secret | CTRL, ENCAPS_KEY, SHARED_KEY | Positive |
| 28 | ML-KEM: error and no-op paths | Invalid command raises ERROR; CTRL=0 is idle | CTRL, STATUS | Negative |
| 29 | ML-KEM: zeroize clears key material | Encaps key and shared key cleared | ZEROIZE | Positive |
| 30 | ML-KEM: engines are independent | ML-KEM command while ML-DSA is parked (and vice versa) | both CTRL/STATUS | Positive |
| **Interrupts** |
| 31 | Interrupts: enables gate the output | Command-done latches notif status but pin stays low when masked | notif_internal, intr_abr_notif | Positive |
| 32 | Interrupts: unmasking exposes a pending event | Enabling notif + global drives the pin | notif_intr_en, global_intr_en | Positive |
| 33 | Interrupts: write-1-to-clear | Write 0 is ignored; write 1 drops the pin | notif_internal_intr | Positive |
| 34 | Interrupts: counters | Command increments notif count and sets incr pulse | notif_intr_count, incr | Positive |
| 35 | Interrupts: software trigger | error/notif trig set status, pin, and counters; trig=0 is a no-op | *_intr_trig | Positive |
| 36 | Interrupts: global mask | Status latches; pin stays low when global enable is 0 | global_intr_en | Positive |
| **Key Vault** |
| 37 | Key Vault: sideload port | kv_push is readable; out-of-range address is rejected | keymgr_tl_socket | Positive |
| 38 | Key Vault: ML-DSA seed read | read_en copies entry 3, HW-clears, overrides MLDSA_SEED; different entry diverges | kv_mldsa_seed_rd_* | Positive |
| 39 | Key Vault: failure paths | Empty entry 31 → KV_READ_FAIL; read_en=0 no-op; out-of-range load_kv_entry ignored | kv status ERROR | Negative |
| 40 | Key Vault: ML-KEM lanes | Seed and msg rd_ctrl VALID; KEYGEN with KV seed completes | kv_mlkem_seed/msg_rd_* | Positive |
| 41 | Key Vault: shared-key export | After ENCAPS, wr_ctrl stores the shared key; export with no key → KV_WRITE_FAIL | kv_mlkem_sharedkey_wr_* | Positive |
| 41a | Key Vault: KM dual-share sideload | XOR reconstruct into KV[0]; palindromic seed matches SW KEYGEN; sequential seed is dword-reversed; lanes 1–3; shred; rejected accesses | keymgr_*_socket | Positive |
| **Backend seam** |
| 42 | Crypto backend seam | Default name `pqclean-ml-dsa-87+ml-kem-1024`; nullptr ignored; SHAKE stand-in round-trips; stub backend supplies recognizable fill patterns | set_crypto_backend | Positive |
| 43 | NIST ACVP KATs | keyGen / sigGen (external-mu, deterministic) / sigVer match `abr_nist_vectors.h` | MLDSA_SEED, PRIVKEY_IN, EXTERNAL_MU, SIGNATURE, VERIFY_RES | Positive |

---

## 3. Coverage notes

- Large key/signature windows are exercised by round-trip tests, not by
  hashing every dword through the engine.
- `PCR_SIGN` hashes `set_pcr_digest()` when set; empty digest falls back to MSG.
- Coverage builds use `./run_tests.sh --coverage` (isolated `build/coverage`).
  Do not combine with `--asan`.

## 4. Platform tests (not in this plan)

`sw/sep-vp-tests/fw-tests-from-tt-oca-hw/fw/sep/tests/sep_abr_*` are
hardware-oriented firmware tests. `abr_ip` is bound into `sep-vp` at
`0x1094_0000` (PIC 35/36). Software-seed tests (`sep_abr_csr_test`,
`sep_abr_keygen_test`, `sep_abr_keygen_sign_test`, `sep_abr_kat_test`,
`sep_abr_nist_kat_test`) can run against the FIPS backend.
`sep_abr_km_seed_test` uses KM DEST `0x10` (ABR ML-DSA seed) which is
bound from `key_manager_model::abr_mldsa_seed_socket`.
