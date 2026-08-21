# Adams Bridge SystemC TLM High-Level Design Specification

**Document Version:** 1.0
**Date:** 2026-08-20
**IP Module:** Adams Bridge (ABR)
**Modeling Approach:** SystemC TLM-2.0 loosely-timed, CSML register file, pluggable crypto backend

---

## Executive Summary

`abr_ip` is a loosely-timed TLM-2.0 model of the Caliptra Adams Bridge PQC
engine as it appears on SEP: one 64 KiB aperture at `0x1094_0000` hosting
independent ML-DSA-87 and ML-KEM-1024 sequencers. Register offsets and
access types come from `abr_reg.rdl`. Timing is quantum-keeper annotated
cycle counts, not a cycle-accurate datapath.

Cryptographic math is deliberately **not** inlined. `abr_crypto_backend`
is the seam: the default `abr_fips_backend` (PQClean ML-DSA-87 +
ML-KEM-1024) is NIST-conformant. `abr_shake_backend` is a deterministic
functional stand-in for tests that do not need ACVP vectors. Firmware
sequencing, interrupts, zeroize and Key Vault tests exercise the FSM
regardless of backend; `sep_abr_nist_kat_test` needs the FIPS default.

This IP is bound into `sep-vp` at `0x1094_0000` with PIC sources 35
(`intr_abr_error`) and 36 (`intr_abr_notif`).

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Features](#2-features)
3. [Port Interfaces](#3-port-interfaces)
4. [Configuration Parameters](#4-configuration-parameters)
5. [Internal Architecture](#5-internal-architecture)
6. [Register Callbacks](#6-register-callbacks)
7. [Modeling Assumptions](#7-modeling-assumptions)
8. [Conclusion](#8-conclusion)

---

## 1. Introduction

### 1.1 Objective

Specify the SystemC TLM implementation of Adams Bridge for virtual-platform
use: software-visible registers, READY/VALID handshake, interrupt
aggregation, Key Vault sideload, and the crypto-backend boundary.

### 1.2 Scope

In scope: MMIO decode, command FSMs, zeroize, interrupts, KV import/export,
LT latency, the default FIPS backend (and SHAKE stand-in), unit tests,
and sep-vp wiring at `0x1094_0000`.

Out of scope: cycle-accurate NTT/SHAKE hardware, SCA masking using
`ABR_ENTROPY`, and a live PCR bank (PCR_SIGN consumes a back-door digest).

### 1.3 Acronyms

| Acronym | Definition |
|---|---|
| ABR | Adams Bridge |
| ACVP | Automated Cryptographic Validation Protocol |
| CSML | internal SystemC register/memory helper library |
| KV | Key Vault |
| LT | Loosely Timed (TLM with temporal decoupling) |
| ML-DSA | Module-Lattice Digital Signature Algorithm (FIPS 204) |
| ML-KEM | Module-Lattice Key-Encapsulation Mechanism (FIPS 203) |
| PQC | Post-Quantum Cryptography |
| SHAKE256 | SHA-3 extendable-output function (FIPS 202) |
| W1C | Write-1-to-clear |

### 1.4 Layering

```
abr_ip          engines, interrupts, KV, quantum keeper   (adams_bridge.*)
  └─ abr_base   csml_memory + every software-visible word (abr_base.*)
       └─ abr_register.h   RO/WO/RW types and CTRL/STATUS bitfields
abr_crypto_backend   math only (abr_crypto.*)
```

This matches `hmac_base` / `kmac_base`: the base class is pure declaration so
a map change lands in one header.

---

## 2. Features

### 2.1 Is (MUST model)

#### 2.1.1 Dual independent sequencers

ML-DSA and ML-KEM each have an `SC_THREAD` woken by their own start event.
A command on one engine does not wait for the other.

#### 2.1.2 Park-on-VALID handshake

Firmware depends on this exact completion model (`abr_mldsa.h`):

```
reset / zeroize : READY=1, VALID=0, ERROR=0
command issued  : READY=0, VALID=0
command done    : VALID=1, READY stays 0
```

Zeroize is the only path that returns `READY=1`. Invalid `CTRL` encodings
(`5..7`) raise `STATUS.ERROR` without producing outputs.

#### 2.1.3 ML-DSA commands

`KEYGEN`, `SIGNING`, `VERIFYING`, `KEYGEN_SIGN`.

- Seed may come from `MLDSA_SEED` or a prior KV seed read.
- Sign uses `MLDSA_PRIVKEY_IN`, or keygen-then-sign from the seed.
- Message representative `mu` is either hashed from MSG/CTX, taken from the
  stream accumulator when `STREAM_MSG` is set, loaded from
  `MLDSA_EXTERNAL_MU`, or (when `PCR_SIGN` is set) hashed from the digest
  installed by `set_pcr_digest()`. An empty PCR digest falls back to MSG.
- Verify writes recomputed c-tilde to `MLDSA_VERIFY_RES`; firmware compares
  it to the signature prefix. There is no boolean pass bit.

#### 2.1.4 ML-KEM commands

`KEYGEN`, `ENCAPS`, `DECAPS`, `KEYGEN_DECAPS`. Shared-secret agreement and
implicit rejection (no `ERROR` on a bad ciphertext) are required of the
backend.

#### 2.1.5 Streaming message

`MLDSA_MSG` writes always append to `m_msg_stream`, because firmware streams
the message **before** writing `CTRL.STREAM_MSG`. `MLDSA_MSG_STROBE` selects
which bytes of each dword are live (little-endian within the word; strobe
bit 0 is bits [7:0]). The
accumulator is capped at 64 KiB.

#### 2.1.6 Zeroize

`CTRL.ZEROIZE` is handled by the engine thread so it costs modeled time and
uses the same busy/ready handshake as a command. It clears every key,
signature, seed, message, context and KV-latched buffer, then restores
`READY`.

#### 2.1.7 Interrupts

Sticky error and notification status, per-source enables, global enables,
W1C, software triggers, and incrementing counters. Pins `intr_abr_error`
and `intr_abr_notif` are driven from a dedicated thread so they have a
single SystemC writer (reset cannot poke the ports directly).

#### 2.1.8 Key Vault sideload

32 × 64-byte slots behind `keymgr_tl_socket` (flat `entry * 64 + byte` map
used by unit tests). The key manager writes HMAC-style dual XOR shares on
four destination sockets that reconstruct into:

| KM DEST bit | Socket | KV entry |
|---|---|---|
| `0x10` ML-DSA seed | `keymgr_mldsa_seed_socket` | 0 (`kv_read[0]`) |
| `0x20` ML-KEM D | `keymgr_mlkem_d_socket` | 1 |
| `0x40` ML-KEM Z | `keymgr_mlkem_z_socket` | 2 |
| `0x80` ML-KEM MSG | `keymgr_mlkem_msg_socket` | 3 |

Firmware `rd_ctrl` copies a slot into the seed/msg override used by the
next command. ML-DSA seed copies reverse the eight dwords to match
`abr_ctrl.sv` (`SEED_NUM_DWORDS-1-dword`). Empty slots fail the read;
`wr_ctrl` exports the last ML-KEM shared key. `read_en` / `write_en`
hardware-clear. `KEY_CTRL=0` shreds the reconstructed entry.

#### 2.1.9 Pluggable crypto

`set_crypto_backend()` replaces the math provider before or during
elaboration/runtime. `nullptr` is ignored so the model always has a backend.
The default reports `is_standards_conformant() == true`.

#### 2.1.10 LT timing

Each operation charges a `csml_param` cycle count converted through `clk_i`
(or `default_clk_freq_hz`) into `tlm_quantumkeeper::inc()`, syncing only
when the quantum is exhausted. Counts are order-of-magnitude, not RTL cycle
accurate.

### 2.2 Is not

- Cycle-accurate NTT, sampling, or Keccak rounds.
- Using `ABR_ENTROPY` for masking or a DRBG (WO storage only; SCA in RTL).
- A live PCR bank: `PCR_SIGN` hashes a back-door digest from
  `set_pcr_digest()`, or MSG if that digest is empty.
- EDN / entropy_src hardware ports.

---

## 3. Port Interfaces

| Port | Type | Role |
|---|---|---|
| `abr_base::memory` target | TLM 32-bit | Register aperture, 64 KiB |
| `keymgr_tl_socket` | `simple_target_socket_optional<abr_ip, 32>` | Flat KV sideload (unit tests; unbound on sep-vp) |
| `keymgr_mldsa_seed_socket` | `simple_target_socket<abr_ip, 32>` | KM DEST 0x10 dual-share |
| `keymgr_mlkem_d_socket` | `simple_target_socket<abr_ip, 32>` | KM DEST 0x20 dual-share |
| `keymgr_mlkem_z_socket` | `simple_target_socket<abr_ip, 32>` | KM DEST 0x40 dual-share |
| `keymgr_mlkem_msg_socket` | `simple_target_socket<abr_ip, 32>` | KM DEST 0x80 dual-share |
| `clk_i` | `sc_in<double>` | Frequency in Hz |
| `rst_ni` | `sc_in<bool>` | Active-low; falling edge clears state |
| `intr_abr_error` | `sc_out<bool>` | SEP PIC 35 |
| `intr_abr_notif` | `sc_out<bool>` | SEP PIC 36 |

`clk_i` may be unbound in register-only benches; `clk_freq()` then uses
`default_clk_freq_hz` (100 MHz).

---

## 4. Configuration Parameters

All knobs are `csml_param` (CCI) and are re-read when used.

| Name | Default | Meaning |
|---|---|---|
| `verbosity` | 2 (Release: 1) | CSML log level |
| `default_clk_freq_hz` | `1e8` | Fallback when `clk_i` is 0/unbound |
| `mldsa_keygen_cycles` | 26000 | Modeled ML-DSA keygen cost |
| `mldsa_sign_cycles` | 78000 | Modeled sign cost |
| `mldsa_verify_cycles` | 30000 | Modeled verify cost |
| `mlkem_keygen_cycles` | 12000 | Modeled ML-KEM keygen cost |
| `mlkem_encaps_cycles` | 15000 | Modeled encaps cost |
| `mlkem_decaps_cycles` | 18000 | Modeled decaps cost |
| `zeroize_cycles` | 64 | Modeled wipe cost |
| `kv_access_cycles` | 32 | Added to sideload `b_transport` delay |

---

## 5. Internal Architecture

```
                    ┌─────────────────────────────────────────┐
  TLM MMIO ────────►│ abr_base::memory  (csml decode)         │
                    │   CTRL write ──► latch cmd / zeroize    │
                    │                  notify start event     │
                    └───────────┬─────────────────┬───────────┘
                                │                 │
                     mldsa_engine_thread   mlkem_engine_thread
                                │                 │
                                ▼                 ▼
                         abr_crypto_backend (PQClean FIPS default)
                                │
                    interrupt_update_thread ◄── raise_notif / raise_error
                                │
                                ▼
                      intr_abr_error / intr_abr_notif

  keymgr_tl_socket              ──► m_kv_entries[32] ──► kv_*_rd/wr_ctrl
  keymgr_{mldsa,mlkem_*}_socket ──► XOR reconstruct  ──► entries 0..3
```

Endianness: SEP firmware and NIST ACVP pack material as little-endian
32-bit words. `regs_to_bytes` / `bytes_to_regs` follow that mapping.

Private-key layout is the FIPS 204 packed `sk` (`ρ || K || tr || s1 || s2 || t0`).
Sign computes `mu` from the `tr` bound into `sk`; verify hashes the public key.

---

## 6. Register Callbacks

| Offset | Callback | Side effect |
|---|---|---|
| `MLDSA_CTRL` | `mldsa_ctrl_write` | Latch modifiers, start engine or error |
| `MLKEM_CTRL` | `mlkem_ctrl_write` | Same for ML-KEM |
| `MLDSA_MSG[i]` | `mldsa_msg_write` | Store word + append strobed bytes to stream |
| `kv_*_rd_ctrl` / `wr_ctrl` | KV handlers | Copy slot, HW-clear enable, set status |
| `error/notif_internal_intr` | W1C | Clear sticky status, recompute pins |
| `error/notif_intr_trig` | W1S | `raise_error` / `raise_notif` |
| `*_intr_en` / `global_intr_en` | enable writes | Recompute pins (unmasking a pending bit asserts) |

All other words use CSML mask-on-read / merge-on-write from
`abr_register.h`.

---

## 7. Modeling Assumptions

1. **Functional math is enough for VP bring-up.** The default FIPS backend
   matches NIST ACVP vectors used by `sep_abr_nist_kat_test`.
2. **`ABR_ENTROPY` is WO storage** so firmware writes do not fail; the
   FIPS backend ignores it (SCA-only in RTL).
3. **`PCR_SIGN` hashes `set_pcr_digest()`** when that buffer is non-empty,
   otherwise MSG. There is no PCR vault in the VP.
4. **KV emptiness is "any non-zero byte".** Hardware may have a valid bit
   per entry; the VP has no separate valid array.
5. **Zeroize of one engine currently wipes shared ABR buffers** (entropy,
   both algorithms' windows). That is conservative and matches "clear all
   key material" in the model comment.
6. **KM dual-share reconstruct** collapses `abr_wrapper_key_reg` +
   `sep_abr_kv_shim` into the four destination sockets.
7. **ML-KEM KV seed read supplies D only.** `kv_mlkem_seed_rd_ctrl` copies
   one 32-byte entry into `m_kv_mlkem_seed`. KEYGEN still takes Z from
   `MLKEM_SEED_Z`. KM DEST `0x40` (Z) and `0x20` (D) land in KV[2] and
   KV[1]; firmware that sideloads Z must still issue a seed read of that
   entry or write `MLKEM_SEED_Z`. There is no `sep_abr_km_mlkem` firmware
   test yet.

---

## 8. Conclusion

Adams Bridge is a firmware-visible LT peripheral with a FIPS 204/203 default
backend, sep-vp bind at `0x1094_0000` (PIC 35/36), and key-manager dual-share
sideload into KV entries 0–3. `ABR_ENTROPY` remains SCA-only (unused by the
math). `PCR_SIGN` uses `set_pcr_digest()` because the VP has no PCR bank.
