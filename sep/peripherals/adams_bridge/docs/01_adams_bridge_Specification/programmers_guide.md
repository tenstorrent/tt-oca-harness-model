# Adams Bridge Programmer's Guide

Firmware sequences against Adams Bridge are defined by the READY/VALID
handshake in `sw/sep-vp-tests/.../common/abr_mldsa.h`. The VP model preserves
that handshake. The default math backend is FIPS 204 / FIPS 203.

## Completion contract

```
reset / zeroize : READY=1, VALID=0, ERROR=0
command issued  : READY=0, VALID=0
command done    : VALID=1, READY stays 0
```

The sequencer **parks at the operation's end state**. `READY` does not
re-assert on completion. Firmware must issue `CTRL.ZEROIZE` between
back-to-back operations and detect that zeroize by polling `READY=1`.

`CTRL` is write-only and reads as 0 after the write is latched.

## ML-DSA keygen

1. Poll `MLDSA_STATUS.READY`.
2. Write 8 dwords to `MLDSA_SEED` (or pull a Key Vault entry first).
3. Optionally write `ABR_ENTROPY`.
4. Write `MLDSA_CTRL.CTRL = KEYGEN`.
5. Poll `MLDSA_STATUS.VALID`.
6. Read `MLDSA_PUBKEY` (648 dwords) and `MLDSA_PRIVKEY_OUT` (1224 dwords).
7. `ZEROIZE` before the next command.

## ML-DSA sign

1. `ZEROIZE` / wait `READY`.
2. Write the private key to `MLDSA_PRIVKEY_IN` (unless using KEYGEN+SIGN).
3. Load the message:
   - fixed 16-word `MLDSA_MSG`, or
   - stream into `MLDSA_MSG` (optionally with `MLDSA_MSG_STROBE`) and set
     `CTRL.STREAM_MSG`, or
   - write `MLDSA_EXTERNAL_MU` and set `CTRL.EXTERNAL_MU`.
4. Write `MLDSA_SIGN_RND`.
5. Optional: `MLDSA_CTX` + `MLDSA_CTX_CONFIG.CTX_SIZE`.
6. Write `CTRL = SIGNING` (or `KEYGEN_SIGN`).
7. Poll `VALID`; read `MLDSA_SIGNATURE`.

Streaming writes are accumulated **before** the command is issued. The
command's `STREAM_MSG` bit decides whether that accumulator or the 16-word
block is hashed.

## ML-DSA verify

Firmware compares `MLDSA_VERIFY_RES` (recomputed c-tilde) against the first
64 bytes of the presented signature. There is no boolean pass/fail bit: a
mismatch is how a bad signature is reported.

## ML-KEM

Same READY/VALID/ZEROIZE contract. `MLKEM_STATUS.ERROR` is bit 2 (there is
no `MSG_STREAM_READY`).

Typical encaps/decaps:

1. KEYGEN → read `MLKEM_ENCAPS_KEY` / `MLKEM_DECAPS_KEY`.
2. ZEROIZE.
3. Write encaps key + `MLKEM_MSG`, issue ENCAPS, read `MLKEM_CIPHERTEXT` and
   `MLKEM_SHARED_KEY`.
4. ZEROIZE.
5. Write decaps key + ciphertext, issue DECAPS, read `MLKEM_SHARED_KEY`.

A ciphertext that does not match is not flagged as `ERROR`. The backend
returns an implicit-rejection shared key (FIPS 203 behaviour).

## Key Vault

1. The key manager writes HMAC-style dual XOR shares (`SHARE0` @ 0x00,
   `SHARE1` @ 0x20, `KEY_CTRL` @ 0x40) into `keymgr_mldsa_seed_socket` (and
   the three ML-KEM dest sockets). ABR reconstructs the seed into KV entry 0
   (D/Z/MSG into entries 1–3). Unit tests may instead write 64-byte slots
   through `keymgr_tl_socket` at `entry * 64`.
2. Firmware sets `kv_*_rd_ctrl.read_en` plus a 5-bit entry index. The ML-DSA
   seed copy reverses dword order to match RTL `abr_ctrl.sv`.
3. Status `VALID` + `ERROR=SUCCESS` means the copy landed; `read_en` is
   already clear.
4. A subsequent command uses the KV material instead of the WO seed/msg
   registers.
5. After ML-KEM encaps/decaps, `kv_mlkem_sharedkey_wr_ctrl` copies the
   latched shared key into a chosen entry.

Empty (all-zero) entries report `KV_READ_FAIL`. Export with no latched
shared key reports `KV_WRITE_FAIL`. `KEY_CTRL=0` shreds the reconstructed
entry.

`PCR_SIGN` hashes the digest installed with `abr_ip::set_pcr_digest()`. An
empty digest falls back to `MLDSA_MSG`. There is no PCR bank on the VP.

## Interrupts

Status bits latch even when masked. The output pins assert only when the
internal enable **and** the matching global enable are set. Writing `1` to
`*_internal_intr` clears the sticky bit. `*_intr_trig` injects an event for
software test.
