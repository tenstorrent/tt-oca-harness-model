# Key Manager SystemC TLM High-Level Design

**Document Version:** 2.0
**Date:** 2026-08-19
**IP Module:** Key Manager (`key_manager_model`, namespace `keymgr_tt`)
**Modeling Approach:** SystemC TLM-2.0 with a firmware-handler abstraction

> **Revision 2.0 note.** Revision 1.0 described the model as it stood against an
> earlier KM RTL revision, which exposed two AXI-Lite slaves to SEP: the mailbox
> and a Key Provisioning Vault Load Port (KPVLP) at `0x10921000`. That window is
> gone from the hardware. Keys now reach the vault only through the mailbox, the
> two commands that drove KPVLP (`0x20`, `0x21`) are retired, key sizes are
> counted in words rather than slots, and handles are issued monotonically. This
> revision rewrites the document against the current model. The specification of
> record is [`01_key_manager_Specification/`](01_key_manager_Specification/) —
> `doc/architecture.adoc` and `doc/firmware.adoc`, with the ROM firmware under
> `dv/fw/`.

---

## Executive Summary

This document specifies the SystemC transaction-level model of the Key Manager.
The model implements a **firmware-handler abstraction**: the PicoRV32 CPU that
runs ROM firmware in hardware is replaced by a C++ class that performs the same
key management operations.

Unlike the RTL, the model:

- **has no processor core or ISS** — no instruction fetch, decode or execute;
- **loads no firmware image** — ROM firmware behaviour lives in `km_firmware_handler`;
- **executes C++ handlers directly** — a complete mailbox frame is dispatched to a
  typed command handler in zero simulated time;
- **preserves the observable interface** — the SEP-visible mailbox registers, the
  IRQ output, the sideload sockets and the reset/wipe inputs all behave as in RTL;
- **draws entropy from OpenSSL `RAND_bytes`** in place of the hardware DRBG, so no
  EDN or DRBG socket is needed.

The result is functional equivalence from the SEP software point of view, which is
what lets SEP firmware be developed and tested against the virtual platform.

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Features](#2-features)
3. [Port Interfaces](#3-port-interfaces)
4. [Memory-Mapped Registers](#4-memory-mapped-registers)
5. [Register Callbacks](#5-register-callbacks)
6. [Internal Architecture](#6-internal-architecture)
7. [Modeling Assumptions](#7-modeling-assumptions)
8. [Conclusion](#8-conclusion)

---

## 1. Introduction

### 1.1 Overview

The Key Manager is a security block that owns key material on behalf of the rest
of the SEP subsystem. It is responsible for:

- a **Key and Policy Vault (KPV)** of 32 slots × 512 bits;
- a **mailbox** through which SEP firmware requests key operations;
- **sideloading keys** into the crypto engines as XOR-masked dual shares;
- responding to **emergency wipe** by zeroing engine keys and halting;
- holding **OTP data** (life-cycle state, chiplet UID) for future KDF work.

The defining architectural point is that SEP never touches key material or the
vault. It asks for operations over the mailbox and receives opaque handles. Every
path into the vault belongs to the KM's own CPU, behind its private crossbar.

### 1.2 Purpose

This document defines the TLM implementation for virtual platform integration: the
features in and out of scope, the external ports, the SEP-visible registers, the
register side-effects, and the assumptions the abstraction rests on.

### 1.3 Modeling Goals

1. **Enable SEP firmware development** with enough fidelity for driver work.
2. **Support system integration** by modelling the external interfaces.
3. **Maintain the security boundary** — slot locking, dual-share sideload, wipe.
4. **Abstract implementation detail** — no ISS, no SRAM images, no RTL timing.

### 1.4 Audience

Model developers, virtual platform integrators, SEP firmware engineers writing KM
driver code, and verification engineers extending the test plan.

---

## 2. Features

### 2.1 Critical Architectural Context

- **No PicoRV32 ISS** — no instruction execution.
- **No ROM or SRAM image loading** — firmware behaviour is compiled into
  `km_firmware_handler.cpp`.
- **OpenSSL `RAND_bytes`** replaces the hardware DRBG; there is no EDN socket.
- **OTP data is pushed** by `set_otp_data()` at `start_of_simulation`, not read
  over a bus port.
- **One SEP-visible window** — the mailbox. Everything else in the block sits
  behind the KM CPU's crossbar and is modelled as C++ state.

### 2.2 Is (MUST model)

#### 2.2.1 Boot Sequence

- One-time boot when `rst_ni` de-asserts.
- Engine key registers zeroed across HMAC, KMAC, AES and OTBN via the sockets.
- `RESP_KM_READY` (0x55) pushed to the outbound FIFO to announce boot.
- IRQ raised if `MB_IRQEN.OUTBOUND_READ_DATA_AVAIL_EN` is set.

The model is held in software reset by SEP at power-on, so a SEP-side test must
release `SEP_RESET_CTRL.SW_RESET_N` bit 0 before the boot message appears.

#### 2.2.2 Mailbox Message Protocol

- Dual FIFOs, inbound (SEP→KM) and outbound (KM→SEP), 16 words deep each.
- A frame is a header word, `PAYLOAD_LEN` payload words, and a CRC-32C word when
  the payload is non-empty. The final word carries the separator flag.
- Header layout: `[31:24]` CRC-8/ROHC over the lower 24 bits, `[23:16]`
  `PAYLOAD_LEN`, `[15:8]` `CMD_ID`, `[7:0]` `SEQ_NUM`.
- Validation order: header CRC-8, sequence number, frame length, payload CRC-32C.
- Error returns: `RET_HEADER_CRC`, `RET_CMD_NOSEQ`, `RET_INVALID_CMD`,
  `RET_INVALID_LEN`, `RET_PAYLOAD_CRC`, `RET_INVALID_ARG`.

**Frames may exceed the FIFO.** A 128-word key is a 131-word frame against a
16-word FIFO. SEP writes it in instalments; the KM drains the FIFO as words
arrive and reassembles them in `m_rx_buffer`, which is sized like the firmware's
SRAM message buffer (`1 + 255 + 1` words). A partial frame therefore survives
between calls to `process_messages()`, and the handler acts only once a separator
completes the frame.

#### 2.2.3 Command Set

| Command | ID | Behaviour |
|---|---|---|
| `CMD_HW_VER` | 0x00 | Hardware version (1.0.0) |
| `CMD_ROM_VER` | 0x01 | ROM firmware version (1.0.0) |
| `CMD_SRAM_VER` | 0x02 | `RET_FAILURE` — no mutable firmware is modelled |
| `CMD_STAT` | 0x03 | Recoverable-fault status bit |
| `CMD_RECOV_ACK` | 0x04 | Acknowledge and clear a recoverable fault |
| `CMD_EXEC_ROM` | 0x10 | Latch ROM-only operation; refuses later handover |
| `CMD_SRAM_LOAD_EXEC` | 0x11 | `RET_FAILURE` — no mutable firmware model |
| `CMD_SRAM_EXEC` | 0x12 | `RET_FAILURE` — no mutable firmware model |
| `CMD_KEY_GENERATE` | 0x22 | Provision a key from the DRBG; returns a handle |
| `CMD_KEY_REVOKE` | 0x23 | Invalidate a handle and return its slots |
| `CMD_KEY_TRANSFER` | 0x24 | Sideload a key to engines as dual shares |
| `CMD_ENGINE_SHRED` | 0x25 | Zero the sideload key registers of selected engines |
| `CMD_KEY_LOAD` | 0x26 | Provision a SEP-supplied key; returns a handle |
| `CMD_ABR_SK_TRANSFER` | 0x27 | `RET_FAILURE` — Adams Bridge is not modelled |
| `CMD_OTP_READ_LOCK_COLD` | 0x28 | Set the cold-domain OTP read lock bits |

`0x20` (`CMD_KPVLP_SLOT_REQ`) and `0x21` (`CMD_KEY_REGISTER`) belonged to the KPVLP
flow and now return `RET_INVALID_CMD`, which the test suite pins deliberately.

#### 2.2.4 Key Provisioning

`CMD_KEY_GENERATE` and `CMD_KEY_LOAD` are one operation with two sources of key
material: the DRBG, or the SEP payload. Both run through a single `load_key()`
helper that mirrors `rom_load_key()` in firmware, which is what stops the two
commands drifting apart.

`KEY_SIZE` in either payload is a **word count minus one**, not a slot count. A
key of `len` words occupies `ceil(len / 16)` consecutive slots, up to 128 words
across 8 slots. The base slot records the span in `EXTEND`; the final slot records
its last populated word in `LAST_DWORD`. Those two fields alone are what the
length is later recovered from.

The ordering inside `load_key()` follows the firmware, and each step matters:

1. Reject a bad length, or a `DEST_VALID` of zero — a key with no permitted
   destination could never be used.
2. Find a run of consecutive available slots, starting from a DRBG-random index.
   A run never straddles the end of the vault.
3. CRC-32C the key material as supplied, and keep it with the handle.
4. **Register the handle before touching the vault**, so that a registry
   exhaustion failure cannot strand a written, write-locked slot with no handle
   referring to it. If a later step fails, the handle is rolled back.
5. Erase the run, write the key, then write-lock the whole span.

Handles are issued monotonically from `m_next_handle` and never recycled: revoking
a key returns its slots to the pool but not its handle number, so the registry is
spent after 255 allocations however much vault space remains.

#### 2.2.5 Key and Policy Vault

- 32 slots × 16 words, plus one control register per slot.
- Control bits, per `km_kpv.rdl` `KEY_CTRL`: `[0]` `lock_write`, `[1]` `lock_use`,
  `[2]` `erase` (self-clearing), `[6:4]` `extend`, `[20:17]` `last_dword`.
- No SEP-facing access of any kind. Every entry point is a KM-firmware one.
- Destination policy is **not** held here. In hardware the vault stores opaque
  blobs and the firmware key registry owns the permitted destinations, so
  `dest_valid` lives in `km_handle_entry_t`.
- `km_find_free_slots()` mirrors `slot_available()`: a slot is available when it is
  neither write-locked nor use-locked and not already assigned.

#### 2.2.6 Key Transfer to Crypto Engines

Four TLM initiator sockets carry keys to the engines. The dual XOR share protocol
of `rom_sideload.c` is modelled: `SHARE0[w]` is a fresh DRBG mask, `SHARE1[w]` is
`key[w] ^ mask`, and a write of 1 to `KEY_CTRL` commits.

| Engine | Words | SHARE0 | SHARE1 | KEY_CTRL |
|---|---|---|---|---|
| HMAC, KMAC, AES | 8 (256-bit) | `0x00`–`0x1C` | `0x20`–`0x3C` | `0x40` |
| OTBN | 12 (384-bit) | `0x000`–`0x02C` | `0x030`–`0x05C` | `0x60` |

Both shares are written across the engine's **full** register width. A key shorter
than that width is padded with fresh DRBG words rather than zeros or stale
contents, so a short key leaves no recoverable residue in the tail.

`lock_use` is asserted across the key's span after the first transfer, so a second
transfer of the same handle is refused — one-shot delivery, as in hardware.

Destination bits 4–7 name the Adams Bridge seed ports (`MLDSA_SEED`, `MLKEM_D`,
`MLKEM_Z`, `MLKEM_MSG`). No ABR model exists, so those destinations are accepted
and the key material is dropped with a log warning rather than failing the
transfer, which keeps firmware sequencing exercisable.

#### 2.2.7 Interrupts and Faults

- One `irq` output, driven by `(MB_IRQS & MB_IRQEN) != 0`.
- Five sources: `OUTBOUND_READ_DATA_AVAIL` and `INBOUND_WRITE_SPACE_AVAIL` are
  level-sensitive; `INBOUND_OVERFLOW`, `OUTBOUND_UNDERFLOW` and `FLUSHED_BY_KM`
  are sticky and write-1-to-clear.
- Recoverable faults: `RESP_RECOVERABLE_FAULT` (0xFE) with a fault code, cleared
  by `CMD_RECOV_ACK`.
- Unrecoverable faults: `RESP_UNRECOVERABLE_FAULT` (0xFF); both FIFOs are flushed
  and the firmware halts.
- Emergency wipe (`wipe_ni`, active low): KPV reset, all engine keys zeroed,
  `UFAULT_WIPE_STATE` reported, firmware halted.

#### 2.2.8 Reset

`rst_ni` clears the registers, the vault, the mailbox and the firmware state. The
sequence counters, the handle registry, `m_rx_buffer`, the `CMD_EXEC_ROM` latch and
the cold OTP read lock are all cleared, and `fw_thread` re-runs the boot sequence
on de-assertion.

#### 2.2.9 OTP and Demotion State

Two interfaces carry the data intended for the KDF:

**Static OTP data**, fuse-burned, from `sep_efuse`: the `km_otp_data_t` struct
(`lc_state`, `chiplet_uid[8]`), delivered by `set_otp_data()` from
`och_sep_ss::start_of_simulation()`.

**Live demotion state**, runtime-writable, from `lc_ctrl`: a `demote_fn_t`
callback returning a packed 4-bit value, `[1:0]` for `DEMOTE_1` and `[3:2]` for
`DEMOTE_2`. Each 2-bit field is differentially encoded as `{~v, v}`, so an
undemoted part reads `0xA` rather than `0`. The KM reads this **at KDF invocation
time**, not at elaboration, because `DEMOTE_1`/`DEMOTE_2` are write-1-to-set
registers written by BL1/BL2 after boot. The default stub returns 0, standing in
for an unconnected port rather than for an undemoted part.

### 2.3 Is Not (excluded)

#### 2.3.1 Processor and Firmware Execution

The PicoRV32 ISS, ROM/SRAM image loading, SRAM scrambler initialisation and
ROM/SRAM parity checking are all absent. `CMD_SRAM_VER`, `CMD_SRAM_LOAD_EXEC` and
`CMD_SRAM_EXEC` report `RET_FAILURE`; `CMD_EXEC_ROM` succeeds, since latching
ROM-only operation is exactly what the model already does.

#### 2.3.2 DPA Countermeasures

The multi-pass random overwrite during the boot vault shred is reduced to a single
pass, as is `KEY_SHRED_ITER` during `CMD_ENGINE_SHRED`. Register blanking and
randomised key rotation timing are hardware details and are not modelled. The dual
XOR share encoding itself **is** modelled, as is DRBG padding of short keys.

#### 2.3.3 KM-Internal Hardware

KMCSR, the KPV registers, the DRBG sampler and the crypto key wrappers are all
KM-CPU-internal in hardware and are modelled as C++ state rather than register
blocks. SEP cannot reach any of them, and `0x10921000` — the old KPVLP window — is
unmapped, so an access there raises a bus error.

#### 2.3.4 Adams Bridge

Sideload destinations 4–7 write HMAC-style dual shares into
`abr_mldsa_seed_socket` / `abr_mlkem_d_socket` / `abr_mlkem_z_socket` /
`abr_mlkem_msg_socket`. `CMD_ABR_SK_TRANSFER` still returns `RET_FAILURE`
(the ML-KEM shared-key capture path is not wired); neither
`RESP_ABR_SHARED_KEY_READY` (0x56) nor the ML-KEM shared-key interrupt is
generated.

#### 2.3.5 Other Unmodelled Paths

- The KM's AXI-Lite pass-through to the system eFuse controller (KM-local
  `0x0001_1000`, remapped by the integrator).
- Separate cold and warm reset domains. The model has one reset input, so
  `OTP_READ_LOCK_COLD` is cleared on any reset where hardware would preserve it
  across a warm one.
- The restricted command set during recoverable-fault recovery: all commands are
  accepted while a fault is outstanding.
- KDF. `lc_state`, `chiplet_uid` and the demotion state are all reachable but not
  yet consumed; `CMD_KEY_GENERATE` uses raw DRBG output.

#### 2.3.6 Timing

Every operation completes at `SC_ZERO_TIME`. There are no clock ports, no clock
domain crossing and no power modelling.

---

## 3. Port Interfaces

| Port / Socket | Type | Direction | Description |
|---|---|---|---|
| `rst_ni` | `sc_in<bool>` | in | Active-low reset. Clears registers, vault, mailbox and firmware state; boot re-runs on de-assertion. |
| `wipe_ni` | `sc_in<bool>` | in | Active-low emergency wipe. Zeroes the vault and every engine key register, then reports an unrecoverable fault. |
| `irq` | `sc_out<bool>` | out | Level-sensitive interrupt to the SEP CPU, asserted while `(MB_IRQS & MB_IRQEN) != 0`. |
| `mailbox_socket` | `simple_target_socket<csml_memory<32>, 32>` | target | The only SEP-visible window: the seven mailbox registers. Base `0x10920000`. |
| `hmac_key_socket` | `simple_initiator_socket<key_manager_model, 32>` | initiator | Dual-share 256-bit sideload into HMAC: 8 + 8 words then `KEY_CTRL`, 17 writes. |
| `kmac_key_socket` | as above | initiator | As HMAC. |
| `aes_key_socket` | as above | initiator | As HMAC. |
| `otbn_key_socket` | as above | initiator | Dual-share 384-bit sideload: 12 + 12 words then `KEY_CTRL`, 25 writes. |

### 3.1 Interface Notes

1. **One target socket.** Revision 1.0 listed a second, `kpvlp_socket`; the
   hardware window it modelled no longer exists.
2. **No EDN or DRBG socket** — entropy comes from `RAND_bytes` through
   `get_random_word()`, which satisfies the `drbg_fn_t` callback.
3. **No OTP bus port** — OTP data arrives through `set_otp_data()`.
4. **No `lc_ctrl` socket** — life-cycle state arrives inside `km_otp_data_t`, and
   demotion state through the `demote_fn_t` callback.
5. **Key sockets are initiators.** The KM pushes keys; the engines expose targets.
6. **Addresses are confirmed**, not placeholders: `KM_MAILBOX_SEP` is
   `0x10920000`–`0x1092001B` in `Args.hpp`, matching the generated register header.

---

## 4. Memory-Mapped Registers

The model exposes exactly one MMIO region to SEP. KM-internal registers are not
accessible, by design rather than by omission.

### 4.1 Mailbox Registers (base `0x1092_0000`, size `0x1C`)

| Register | Offset | Access | Description |
|---|---|---|---|
| `MB_WDATA` | 0x00 | WO | Push one word into the inbound FIFO. To mark the last word of a frame, SEP sets `MB_WSEP.SET` first. |
| `MB_WSEP` | 0x04 | RW | Bit 0 tags the next `MB_WDATA` push as the separator. Self-clears on the push. |
| `MB_RDATA` | 0x08 | RO | Pop one word from the outbound FIFO. `MB_STATUS.OUTBOUND_SEPARATOR` reports whether it ended a frame. |
| `MB_STATUS` | 0x0C | RO, W1C `[23:20]` | FIFO fill state, depths and sticky overflow/underflow flags. |
| `MB_IRQS` | 0x10 | RO `[1:0]`, W1C `[4:2]` | Interrupt status. |
| `MB_IRQEN` | 0x14 | RW | Interrupt enable mask. |
| `MB_CTRL` | 0x18 | RW | Bit 2 flushes both FIFOs and self-clears; bits `[1:0]` configure the overflow/underflow response. |

#### MB_STATUS bits

| Bits | Field |
|---|---|
| [0] | `INBOUND_EMPTY` |
| [1] | `INBOUND_FULL` |
| [2] | `OUTBOUND_EMPTY` |
| [3] | `OUTBOUND_FULL` |
| [11:4] | `INBOUND_DEPTH` |
| [19:12] | `OUTBOUND_DEPTH` |
| [20] | `INBOUND_OVERFLOW` (W1C) |
| [21] | `OUTBOUND_OVERFLOW` (W1C) |
| [22] | `INBOUND_UNDERFLOW` (W1C) |
| [23] | `OUTBOUND_UNDERFLOW` (W1C) |
| [24] | `INBOUND_SEPARATOR` |
| [25] | `OUTBOUND_SEPARATOR` |

#### MB_IRQS / MB_IRQEN bits

| Bit | Field | Type |
|---|---|---|
| [0] | `OUTBOUND_READ_DATA_AVAIL` | level |
| [1] | `INBOUND_WRITE_SPACE_AVAIL` | level |
| [2] | `INBOUND_OVERFLOW` | sticky, W1C |
| [3] | `OUTBOUND_UNDERFLOW` | sticky, W1C |
| [4] | `FLUSHED_BY_KM` | sticky, W1C |

### 4.2 Not SEP-Accessible

KMCSR, the KPV key and control registers, the DRBG sampler registers and the OTP
shadow registers are all behind the KM CPU's crossbar. In this model they are C++
state inside `km_firmware_handler` and `km_kpv`; there is no address by which SEP
could reach them.

---

## 5. Register Callbacks

Side-effects are CSML read/write callbacks registered in
`key_manager_model::register_all_callbacks()`.

| Callback | Type | Description |
|---|---|---|
| `handle_write_MB_WDATA` | write | Push the word into the inbound FIFO, applying a pending `MB_WSEP.SET` as the separator. Sets `INBOUND_OVERFLOW` if the FIFO is full, self-clears `MB_WSEP`, and notifies the firmware thread. |
| `handle_write_MB_WSEP` | write | Record the `SET` bit so the next `MB_WDATA` push is tagged. |
| `handle_read_MB_RDATA` | read | Pop one word from the outbound FIFO; sets `OUTBOUND_UNDERFLOW` when empty. |
| `handle_read_MB_STATUS` | read | Return live FIFO state rather than stored contents. |
| `handle_write_MB_STATUS` | write | W1C over bits `[23:20]`, then re-read live state. |
| `handle_write_MB_IRQS` | write | W1C over bits `[4:2]`, clearing the matching sticky mailbox flags, then re-evaluate the IRQ. |
| `handle_write_MB_IRQEN` | write (lambda) | Store the mask and re-evaluate the IRQ immediately. |
| `handle_write_MB_CTRL` | write | On bit 2, flush both FIFOs and self-clear; store bits `[1:0]`. |

The KPVLP callbacks listed in revision 1.0 (`handle_write_KPVLP_KEY`,
`handle_write_KPVLP_CTRL`, `handle_read_KPVLP_STATUS`) no longer exist.

### 5.1 IRQ Update

`irq_update_process()` is an `SC_METHOD` on `m_irq_update_event`. It evaluates
`(MB_IRQS & MB_IRQEN) != 0` and drives `irq`. `update_level_irq_bits()` refreshes
the two level-sensitive bits before the event is notified.

Note that `handle_write_MB_WDATA` notifies the firmware thread on **every** word,
not only on a separator. That is what allows a frame longer than the FIFO to be
drained as it arrives instead of deadlocking against a full FIFO.

---

## 6. Internal Architecture

### 6.1 Class Structure

```
key_manager_model (sc_module, extends key_manager_base)
├── keymgr_tt::km_kpv      m_kpv       — vault storage (pure C++)
├── keymgr_tt::km_mailbox  m_mailbox   — dual FIFOs plus sc_event
└── unique_ptr<keymgr_tt::km_firmware_handler> m_firmware
      ├── km_kpv&        m_kpv          (reference to the model's vault)
      ├── km_mailbox&    m_mailbox      (reference to the model's mailbox)
      ├── drbg_fn_t      m_get_random   (lambda over RAND_bytes)
      ├── key_xfer_fn_t  m_key_xfer     (lambda over key_transfer_via_socket)
      ├── demote_fn_t    m_get_demote   (wired to lc_ctrl; stub returns 0)
      ├── km_otp_data_t  m_otp_data     (pushed at start_of_simulation)
      ├── km_handle_entry_t m_handle_registry[256]
      └── vector<uint32_t>  m_rx_buffer  (inbound frame reassembly)
```

`key_manager_base` owns one `csml_memory<32>` and the single `mailbox_socket`.

### 6.2 SystemC Processes

| Process | Type | Trigger | Description |
|---|---|---|---|
| `fw_thread` | `SC_THREAD` | inbound mailbox event, `rst_ni` | Boot, then the message loop. Leaves the loop to re-boot on reset. |
| `reset_process` | `SC_METHOD` | `rst_ni` | Clears registers, vault, mailbox and firmware state. |
| `wipe_process` | `SC_METHOD` | `wipe_ni` | Emergency wipe: vault reset, engines zeroed, firmware halted. |
| `irq_update_process` | `SC_METHOD` | `m_irq_update_event` | Drives `irq`. |

### 6.3 Message Flow

```
SEP writes MB_WSEP.SET = 1
SEP writes MB_WDATA (final word)      → separator set
    → each write notifies the inbound event
    → fw_thread wakes
    → km_firmware_handler::process_messages()
        → receive_message()   accumulate into m_rx_buffer; return only on separator
        → validate header CRC-8
        → validate sequence number
        → validate frame length
        → validate payload CRC-32C
        → dispatch handle_cmd_*()
        → push_message()      RESP_CMD into the outbound FIFO
    → update_level_irq_bits() → irq asserts if MB_IRQEN[0] is set
SEP reads MB_RDATA until the separator
```

### 6.4 Sideload

`key_transfer_via_socket()` walks the engine table and, for each engine named in
`dest_mask`, writes `max_words` mask words to SHARE0, `max_words` masked key words
to SHARE1, then `KEY_CTRL` — 1 to commit a key, 0 to clear `key_valid` for a
shred. Words beyond the key length are DRBG-random. ABR destinations are logged
and dropped.

---

## 7. Modeling Assumptions

### 7.1 Firmware Abstraction

1. **A C++ handler replaces the ISS.** Boot, command dispatch and fault handling
   all live in `km_firmware_handler.cpp`. The model always runs in ROM mode.
2. **Mutable firmware is not modelled**, so the SRAM commands fail rather than
   pretending to load an image.
3. **Everything completes at `SC_ZERO_TIME`.** Timing is not a goal.

### 7.2 Entropy

4. **`RAND_bytes`** backs key generation, share masking, short-key padding, erase
   fill and the random starting slot. It is unpredictable but not tied to any
   hardware DRBG seed, so sequences are not reproducible across runs.

### 7.3 Storage and Transfer

5. **`lock_use` after first transfer** — a second transfer of the same handle is
   refused, matching one-shot delivery.
6. **Geometry is authoritative.** A key's length is recovered from `EXTEND` on the
   base slot and `LAST_DWORD` on the final slot, never from a stored length.
7. **Handles are monotonic.** Revoking frees slots but not handle numbers.
8. **Destination policy is firmware state**, held in `km_handle_entry_t` beside the
   handle rather than in a vault control register.

### 7.4 Faults and Reset

9. **An unrecoverable fault halts the firmware** until a reset restarts `fw_thread`.
10. **Wipe is synchronous** — `wipe_process()` completes its zeroing before
    returning.
11. **Reset restarts boot**, including re-announcing `RESP_KM_READY`.

### 7.5 OTP and Demotion

12. **OTP data is static**, pushed once at `start_of_simulation` from `sep_efuse`.
13. **Demotion state is live**, read through a callback at KDF invocation time
    because BL1/BL2 write it after boot.
14. **KDF is deferred.** The inputs are all reachable but unused.

### 7.6 Address Map

15. **Mailbox `0x10920000`–`0x1092001B`**, confirmed against
    `KM_MAILBOX_SEP_REG_MAP` in the generated header.
16. **`0x10921000` is unmapped.** The old KPVLP window was removed from the
    platform target list, taking `och_sep_ss::TARG_COUNT` down by one.

---

## 8. Conclusion

### 8.1 Summary

The model captures the SEP-visible behaviour of the Key Manager through a
firmware-handler abstraction that removes the PicoRV32 ISS while preserving the
command protocol, the key lifecycle and sideload delivery.

1. **Firmware-handler architecture** — C++ command handlers, no ISS, no images.
2. **Fifteen commands**, with header CRC-8, payload CRC-32C and sequence checking,
   and frame reassembly for payloads wider than the FIFO.
3. **A single SEP window.** Keys enter the vault only through `CMD_KEY_LOAD`.
4. **One provisioning path** shared by generate and load, ordered as the firmware
   orders it, with monotonic handles and per-key CRC.
5. **Dual-share sideload** to HMAC, KMAC, AES, OTBN and the four ABR dests, padded with DRBG words, one-shot.
6. **Fault handling** — recoverable, unrecoverable, emergency wipe.
7. **OTP and demotion inputs** wired from `sep_efuse` and `lc_ctrl`.

### 8.2 Known Gaps

| Gap | Risk | Notes |
|---|---|---|
| Adams Bridge shared-key capture | Medium | Destinations 4–7 sideload into ABR sockets. `CMD_ABR_SK_TRANSFER` still fails; `RESP_ABR_SHARED_KEY_READY` is never sent. |
| Cold and warm reset domains not separated | Medium | One reset input, so `OTP_READ_LOCK_COLD` clears on a warm reset where hardware would hold it. |
| eFuse pass-through not modelled | Low | KM-local `0x0001_1000` window; no VP path from KM to the eFuse controller. |
| No restricted command set during recovery | Medium | All commands accepted while a recoverable fault is outstanding; the spec restricts this to `CMD_RECOV_ACK`. |
| Mutable firmware not modelled | Low | The three SRAM/ROM execution commands fail by design. |
| KDF not implemented | Low | Inputs reachable but unused; generate returns raw DRBG output. |
| DPA countermeasures reduced | Low | Single-pass shred; share encoding and DRBG padding are modelled. |

---

**End of High-Level Design**
