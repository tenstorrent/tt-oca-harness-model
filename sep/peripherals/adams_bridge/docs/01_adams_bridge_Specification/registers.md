# Adams Bridge Register Map

Byte offsets are relative to the ABR aperture base (`0x1094_0000` on SEP).
Word arrays are listed as *base + N dwords*. Access types follow the RDL:
**WO** reads as 0, **RO** ignores writes, **RW** round-trips, **W1C** clears
on writing 1.

## ML-DSA and common (`0x0000`)

| Offset | Name | Words | Access | Notes |
|---:|---|---:|---|---|
| `0x0000` | `MLDSA_NAME` | 2 | RO | `"MLDSA-87"` in RTL byte-pair-swapped layout: `0x44534D4C`, `0x3837412D` |
| `0x0008` | `MLDSA_VERSION` | 2 | RO | Model seeds `0x00000001` / `0x00000000` |
| `0x0010` | `MLDSA_CTRL` | 1 | WO | Command + modifiers; hardware-cleared |
| `0x0014` | `MLDSA_STATUS` | 1 | RO | READY / VALID / MSG_STREAM_READY / ERROR |
| `0x0018` | `ABR_ENTROPY` | 16 | WO | 512-bit SCA entropy (accepted, not consumed by the default backend) |
| `0x0058` | `MLDSA_SEED` | 8 | WO | 256-bit keygen seed |
| `0x0078` | `MLDSA_SIGN_RND` | 8 | WO | 256-bit signing randomizer |
| `0x0098` | `MLDSA_MSG` | 16 | WO | 512-bit message block, or stream source |
| `0x00D8` | `MLDSA_VERIFY_RES` | 16 | RO | 512-bit recomputed c-tilde |
| `0x0118` | `MLDSA_EXTERNAL_MU` | 16 | WO | 512-bit precomputed mu |
| `0x0158` | `MLDSA_MSG_STROBE` | 1 | WO | 4-bit byte strobe for streamed MSG writes |
| `0x015C` | `MLDSA_CTX_CONFIG` | 1 | WO | `CTX_SIZE` (8 bits), context length in bytes |
| `0x0160` | `MLDSA_CTX` | 64 | WO | Context string (up to 255 bytes used) |
| `0x1000` | `MLDSA_PUBKEY` | 648 | RW | 2592 B public key |
| `0x2000` | `MLDSA_SIGNATURE` | 1157 | RW | 4628 B signature |
| `0x4000` | `MLDSA_PRIVKEY_OUT` | 1224 | RO | 4896 B private key produced by keygen |
| `0x6000` | `MLDSA_PRIVKEY_IN` | 1224 | WO | 4896 B private key consumed by sign |

### `MLDSA_CTRL` (`0x0010`)

| Bits | Field | Encoding |
|---|---|---|
| `[2:0]` | `CTRL` | `0` none, `1` KEYGEN, `2` SIGNING, `3` VERIFYING, `4` KEYGEN_SIGN; `5..7` error |
| `[3]` | `ZEROIZE` | Wipe key material and return to RESET (`READY=1`) |
| `[4]` | `PCR_SIGN` | Hash `set_pcr_digest()` when non-empty, else MSG |
| `[5]` | `EXTERNAL_MU` | Use `MLDSA_EXTERNAL_MU` instead of hashing MSG/CTX |
| `[6]` | `STREAM_MSG` | Consume the streamed-message accumulator |

### `MLDSA_STATUS` (`0x0014`)

Reset value `0x5` (`READY | MSG_STREAM_READY`).

| Bit | Field |
|---|---|
| 0 | `READY` |
| 1 | `VALID` |
| 2 | `MSG_STREAM_READY` |
| 3 | `ERROR` |

## ML-DSA Key Vault (`0x8000`)

| Offset | Name | Access | Notes |
|---:|---|---|---|
| `0x8000` | `kv_mldsa_seed_rd_ctrl` | RW | `read_en` (bit 0), `read_entry` (bits `[5:1]`); `read_en` HW-clears |
| `0x8004` | `kv_mldsa_seed_rd_status` | RO | `READY`, `VALID`, `ERROR` (`0` success, `1` read fail, `2` write fail) |

## Interrupt block (`0x8100`)

Caliptra aggregated interrupt layout. Error = PIC source 35, notification
(command done) = PIC source 36.

| Offset | Name | Access |
|---:|---|---|
| `0x8100` | `global_intr_en_r` | RW — `error_en` bit 0, `notif_en` bit 1 |
| `0x8104` | `error_intr_en_r` | RW — `en` bit 0 |
| `0x8108` | `notif_intr_en_r` | RW — `en` bit 0 |
| `0x810C` | `error_global_intr_r` | RO — aggregated error status |
| `0x8110` | `notif_global_intr_r` | RO — aggregated notif status |
| `0x8114` | `error_internal_intr_r` | W1C — sticky error status |
| `0x8118` | `notif_internal_intr_r` | W1C — sticky command-done status |
| `0x811C` | `error_intr_trig_r` | W1S — software error inject (self-clearing) |
| `0x8120` | `notif_intr_trig_r` | W1S — software notif inject (self-clearing) |
| `0x8200` | `error_intr_count` | RW — error event counter |
| `0x8280` | `notif_intr_count` | RW — command-done event counter |
| `0x8300` | `error_intr_count_incr` | RO pulse |
| `0x8304` | `notif_intr_count_incr` | RO pulse |

Output equation:

```
intr_abr_error = error_internal.sts && error_intr_en.en && global_intr_en.error_en
intr_abr_notif = notif_internal.sts && notif_intr_en.en && global_intr_en.notif_en
```

## ML-KEM (`0x9000`)

| Offset | Name | Words | Access | Notes |
|---:|---|---:|---|---|
| `0x9000` | `MLKEM_NAME` | 2 | RO | `"MLKEM-10"` layout: `0x4B454D4C`, `0x31304D2D` |
| `0x9008` | `MLKEM_VERSION` | 2 | RO | Same version words as ML-DSA |
| `0x9010` | `MLKEM_CTRL` | 1 | WO | `CTRL[2:0]`, `ZEROIZE[3]`; no ML-DSA modifiers |
| `0x9014` | `MLKEM_STATUS` | 1 | RO | `READY[0]`, `VALID[1]`, `ERROR[2]` (no MSG_STREAM_READY) |
| `0x9018` | `MLKEM_SEED_D` | 8 | WO | Keygen seed d |
| `0x9038` | `MLKEM_SEED_Z` | 8 | WO | Implicit-rejection seed z |
| `0x9058` | `MLKEM_SHARED_KEY` | 8 | RO | 256-bit shared secret |
| `0x9080` | `MLKEM_MSG` | 8 | WO | Encapsulation message |
| `0xA000` | `MLKEM_DECAPS_KEY` | 792 | RW | 3168 B |
| `0xB000` | `MLKEM_ENCAPS_KEY` | 392 | RW | 1568 B |
| `0xB800` | `MLKEM_CIPHERTEXT` | 392 | RW | 1568 B |

### `MLKEM_CTRL` (`0x9010`)

| Bits | Field | Encoding |
|---|---|---|
| `[2:0]` | `CTRL` | `0` none, `1` KEYGEN, `2` ENCAPS, `3` DECAPS, `4` KEYGEN_DECAPS; `5..7` error |
| `[3]` | `ZEROIZE` | Wipe ML-KEM (and shared ABR) key material, return `READY=1` |

## ML-KEM Key Vault (`0xC000`)

| Offset | Name | Role |
|---:|---|---|
| `0xC000` / `0xC004` | seed `rd_ctrl` / `rd_status` | Import keygen seed d |
| `0xC008` / `0xC00C` | msg `rd_ctrl` / `rd_status` | Import encapsulation message |
| `0xC010` / `0xC014` | sharedkey `wr_ctrl` / `wr_status` | Export last shared key |

`read_en` / `write_en` is bit 0; entry index is bits `[5:1]` (32 entries).
The enable bit hardware-clears after the copy.
