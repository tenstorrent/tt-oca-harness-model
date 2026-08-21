# Adams Bridge HWIP — Programmer-Facing Specification

**Document Version:** 1.0
**Date:** 2026-08-20
**IP Module:** Adams Bridge (ABR)
**Source of offsets:** `tt-oca-hw/vendor/adams_bridge/src/abr_top/rtl/abr_reg.rdl`
(transcribed into `include/abr_base.h` / `include/abr_register.h`)

This folder is the VP-side specification snapshot for the Adams Bridge
peripheral. It is **not** a copy of the Caliptra ABR RTL tree. Firmware-visible
offsets, field encodings, and the READY/VALID handshake are taken from the
vendored RDL; cryptographic algorithm details are FIPS 203/204 as implemented
by a pluggable backend (see the [high-level design](../02_adams_bridge_HighLevel_Design.md)).

## Overview

Adams Bridge is the post-quantum crypto accelerator in the SEP crypto
subsystem (`tt-oca-hw/hw/sep/sep_crypto.sv`, gated by `SEP_ABR_EN`). One 64 KiB
aperture serves two independent sequencers:

- **ML-DSA-87** (FIPS 204 Dilithium) — keygen, sign, verify
- **ML-KEM-1024** (FIPS 203 Kyber) — keygen, encapsulate, decapsulate

The two engines do not stall each other. Firmware talks to each through its
own `*_CTRL` / `*_STATUS` pair and a set of key / message / signature windows.

## Features (software-visible)

- Identity registers: `MLDSA_NAME` reads `"MLDSA-87"`, `MLKEM_NAME` reads `"MLKEM-10"`
- Write-only command registers that hardware-clear once latched
- Park-on-VALID completion: `READY` stays low after a command; firmware must
  `ZEROIZE` to return to idle (`READY=1`)
- Optional ML-DSA modifiers: `EXTERNAL_MU`, `STREAM_MSG`, `PCR_SIGN`
- Caliptra-style interrupt block (error + notification, enables, W1C, counters, SW trigger)
- Key Vault sideload: seed/message import and ML-KEM shared-key export
- Zeroize that wipes key material and returns the sequencer to RESET

## Parameter sizes (bytes)

| Object | Size |
|---|---:|
| ML-DSA seed / sign randomizer | 32 |
| ML-DSA mu / c-tilde | 64 |
| ML-DSA public key | 2592 |
| ML-DSA private key | 4896 |
| ML-DSA signature | 4628 |
| ML-KEM seed D/Z, message, shared key | 32 |
| ML-KEM encapsulation key / ciphertext | 1568 |
| ML-KEM decapsulation key | 3168 |

## Documents in this folder

- [registers.md](registers.md) — offset table and field encodings
- [programmers_guide.md](programmers_guide.md) — command sequences firmware must follow

## Related firmware tests

`sw/sep-vp-tests/fw-tests-from-tt-oca-hw/fw/sep/tests/sep_abr_*` exercise this
model on `sep-vp` at `0x1094_0000`. The default PQClean backend is expected to
match NIST ACVP vectors (`sep_abr_nist_kat_test`). `sep_abr_km_seed_test` uses
key-manager DEST `0x10` (ML-DSA seed).
