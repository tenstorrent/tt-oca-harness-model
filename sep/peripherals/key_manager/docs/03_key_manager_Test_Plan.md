# Key Manager SystemC TLM Test Plan

**Document Version:** 2.0
**Date:** 2026-08-19
**IP Module:** Key Manager (`key_manager_model`, namespace `keymgr_tt`)
**Scope:** the IP-level SystemC testbench, `test/src/key_manager_func*_test.cpp`

> **Revision 2.0 note.** Revision 1.0 described a 13-file, ~45 sub-test suite built
> around the KPVLP provisioning window: `func005` exercised `CMD_KPVLP_SLOT_REQ`,
> `func006` exercised `CMD_KEY_REGISTER`, and `func001` checked the KPVLP registers.
> That hardware no longer exists. The suite is now **14 files and 79 sub-tests**,
> with `func005` repurposed to pin the retired command surface and `func006`
> rewritten around `CMD_KEY_LOAD`. This revision documents what the suite actually
> tests today.

---

## Scope

This plan covers the IP-level testbench only — the `key_manager_test` binary built
by `run_tests.sh`, which drives the model directly over its TLM sockets with
recording stubs standing in for the crypto engines.

Platform-level firmware tests that reach the Key Manager through the SEP CPU and
bus live in `sw/sep-vp-tests/` and are documented with the platform test suite, not
here.

---

## Executive Summary

The model implements a firmware-handler abstraction: a C++ class replaces the
PicoRV32 ISS while preserving the SEP-visible behaviour of the block. The suite
therefore verifies the observable contract — register semantics, mailbox framing,
command results, key lifecycle, sideload encoding, faults and reset — and not
instruction execution.

### Testing Focus

1. **Register interface** — the seven mailbox registers, their access types and reset values.
2. **Mailbox protocol** — CRC-8 header, CRC-32C payload, sequence enforcement, separator framing, and reassembly of frames wider than the FIFO.
3. **Boot** — `RESP_KM_READY`, engine key zeroing, IRQ behaviour.
4. **Command set** — all fifteen commands, plus the two retired IDs and error paths for malformed frames.
5. **Key lifecycle** — load or generate → transfer → revoke, including multi-slot keys.
6. **Sideload** — dual XOR shares, DRBG padding of short keys, one-shot `lock_use`.
7. **Faults** — recoverable, unrecoverable, emergency wipe, firmware halt.
8. **Reset** — state cleared, handles invalidated, firmware re-boots.
9. **Interrupts** — `MB_IRQS & MB_IRQEN`, level versus sticky sources.

### Architectural Context

This suite is written for the firmware-handler model, so several expectations
differ from an RTL testbench by design:

- firmware is C++ in `km_firmware_handler.cpp`, not an ISS, so the SRAM execution commands fail rather than loading an image;
- entropy is OpenSSL `RAND_bytes`, so generated key values are unpredictable and are verified structurally rather than by value;
- DPA countermeasures are reduced to a single pass, and the tests match the model rather than the hardware;
- there is **one** SEP-visible window. Keys enter the vault only through `CMD_KEY_LOAD`, and there is no test path that writes vault storage directly.

---

## Test Plan Summary

| Category | File | Sub-tests | Count | Focus |
|---|---|---|---|---|
| Register interface | func001 | 001a–001f | 6 | Reset values, RW/WO/RO, W1C, masked writes |
| Mailbox framing and IRQ | func002 | 002a–002g | 7 | End-to-end command, IRQ assert/de-assert, underflow W1C, WSEP self-clear |
| Boot state and info | func003 | 003a–003d | 4 | `CMD_STAT`, `CMD_ROM_VER`, `CMD_SRAM_VER`, `CMD_HW_VER` |
| Frame validation | func004 | 004a–004f | 6 | Header CRC, length, payload CRC, sequence, unknown command, recovery |
| Command surface | func005 | 005a–005h | 8 | Retired `0x20`/`0x21`, and the ROM/SRAM/ABR/OTP commands |
| `CMD_KEY_LOAD` | func006 | 006a–006k | 11 | Provisioning, handles, argument validation, multi-slot keys |
| `CMD_KEY_GENERATE` | func007 | 007a–007e | 5 | Word-count semantics, transfer of generated key, exhaustion |
| `CMD_KEY_TRANSFER` | func008 | 008a–008e | 5 | Sideload, `DEST_VALID` policy, one-shot `lock_use` |
| `CMD_KEY_REVOKE` | func009 | 009a–009c | 3 | Revocation, double revoke, payload length |
| `CMD_ENGINE_SHRED` | func010 | 010a–010c | 3 | Engine key zeroing, multi-engine, length check |
| Full lifecycle | func011 | 011a–011c | 3 | Chained flows, sequence continuity |
| Reset during operation | func012 | 012a–012c, 012c-extra | 4 | Handle invalidation, sequence reset, FIFO cleared |
| Dual-share structure | func013 | 013a | 1 | Share encoding intact for a generated key |
| Extended coverage | func014 | 014a–014m | 13 | Wipe, FIFO faults, exhaustion, policy, vault internals, geometry, handles |
| **Total** | **14 files** | | **79** | |

---

## Test Case Details

### Register interface — func001

The mailbox is the entire SEP-visible register surface, so there is nothing else
to cover here.

| Sub-test | Description |
|---|---|
| 001a | All mailbox registers read their reset values: `MB_STATUS` = 0x5 (both FIFOs empty), `MB_IRQS` = 0, `MB_IRQEN` = 0, `MB_CTRL` = 0. Boot must have completed and `RESP_KM_READY` been consumed first. |
| 001b | `MB_IRQEN` is read/write, and only the writable bits stick. |
| 001c | `MB_CTRL` is read/write; the FLUSH bit self-clears. |
| 001d | `MB_IRQS` sticky bits `[4:2]` are write-1-to-clear; the level bits `[1:0]` are not clearable by writing. |
| 001e | `MB_WDATA` is write-only — a read returns 0. |
| 001f | `MB_STATUS` is read-only apart from the W1C field; other bits are unaffected by a write. |

### Mailbox framing and IRQ — func002

| Sub-test | Description |
|---|---|
| 002a | `CMD_HW_VER` end to end: framed header with correct CRC-8 and separator, then a `RESP_CMD` carrying `RET_SUCCESS`. |
| 002b | The return argument encodes hardware version 1.0.0. |
| 002c | With `MB_IRQEN[0]` set, `irq` asserts once the response is queued, before it is read. |
| 002d | `irq` de-asserts after the outbound FIFO is drained. |
| 002e | Reading an empty outbound FIFO sets the `OUTBOUND_UNDERFLOW` sticky bit. |
| 002f | Writing 1 clears that sticky bit, in both `MB_IRQS` and `MB_STATUS`. |
| 002g | `MB_WSEP` self-clears after tagging one word, so the next word is not also a separator. |

### Boot state and info commands — func003

| Sub-test | Description |
|---|---|
| 003a | `CMD_STAT` after boot reports no recoverable fault. |
| 003b | `CMD_ROM_VER` returns `RET_SUCCESS` and 0x00010000. |
| 003c | `CMD_SRAM_VER` returns `RET_FAILURE`; no mutable firmware is modelled. |
| 003d | `CMD_HW_VER` returns `RET_SUCCESS` and 0x00010000. |

### Frame validation — func004

| Sub-test | Description |
|---|---|
| 004a | A flipped bit in the header CRC gives `RET_HEADER_CRC`, and the sequence counter is not advanced. |
| 004b | A declared payload length longer than the frame carries gives `RET_INVALID_LEN`. |
| 004c | A corrupted payload CRC word gives `RET_PAYLOAD_CRC`. |
| 004d | An out-of-order sequence number gives `RET_CMD_NOSEQ`. |
| 004e | An unknown command ID gives `RET_INVALID_CMD`. |
| 004f | After a `RET_CMD_NOSEQ`, the same sequence number still succeeds — proving the rejected frame did not advance the counter. |

### Command surface — func005

This file guards an interface change rather than a feature. The new RTL removed
SEP's direct access to the vault, retiring two commands; both must now look like
unknown commands. It also pins the commands added alongside `CMD_KEY_LOAD` to
definite outcomes instead of leaving them silently unknown.

| Sub-test | Description |
|---|---|
| 005a | Retired `CMD_KPVLP_SLOT_REQ` (0x20) → `RET_INVALID_CMD`. |
| 005b | Retired `CMD_KEY_REGISTER` (0x21) → `RET_INVALID_CMD`. |
| 005c | `CMD_EXEC_ROM` (0x10) → `RET_SUCCESS`; ROM-only operation is latched. |
| 005d | `CMD_SRAM_LOAD_EXEC` (0x11) → `RET_FAILURE`. |
| 005e | `CMD_SRAM_EXEC` (0x12) → `RET_FAILURE`. |
| 005f | `CMD_ABR_SK_TRANSFER` (0x27) → `RET_FAILURE`; no Adams Bridge model. |
| 005g | `CMD_OTP_READ_LOCK_COLD` (0x28) sets lock bits and accumulates them. |
| 005h | The same command with an empty payload → `RET_INVALID_LEN`. |

### CMD_KEY_LOAD — func006

`CMD_KEY_LOAD` (0x26) is the provisioning path that replaced the KPVLP window:
SEP hands the key over the mailbox and the KM places it in the vault, returning an
opaque handle whose low byte feeds straight into `CMD_KEY_TRANSFER`. Rejections
report `RET_INVALID_ARG` and carry the index of the offending payload word,
checked in the order the firmware validates them.

| Sub-test | Description |
|---|---|
| 006a | An 8-word key loads successfully. |
| 006b | The return argument's low byte is a handle in `[1..255]`. |
| 006c | A payload shorter than the minimum three words → `RET_INVALID_ARG`, argument 0. |
| 006d | A declared key size longer than the frame carries → `RET_INVALID_ARG`, argument 0. |
| 006e | `KEY_SIZE` reserved bits `[31:7]` set → `RET_INVALID_ARG`, argument 0. |
| 006f | A `DEST_VALID` of zero → `RET_INVALID_ARG`, argument 1. |
| 006g | `DEST_VALID` reserved bits `[31:8]` set → `RET_INVALID_ARG`, argument 1. |
| 006h | A one-word key is accepted, being the minimum legal size. |
| 006i | Successive loads return distinct handles. |
| 006j | A 16-word key fills exactly one slot. |
| 006k | A 17-word key spans two slots and reads back intact. |

### CMD_KEY_GENERATE — func007

`KEY_SIZE` is a word count minus one, not a slot count: the firmware draws
`KEY_SIZE + 1` words from the DRBG and hands them to the same loader
`CMD_KEY_LOAD` uses.

| Sub-test | Description |
|---|---|
| 007a | A one-word generate succeeds; the return argument carries the handle in `[7:0]`, the echoed size in `[14:8]` and `DEST_VALID` in `[23:16]`. |
| 007b | A following transfer to HMAC delivers non-zero key material to the recording stub. |
| 007c | Generating keys until the vault cannot fit another → `RET_FAILURE`. |
| 007d | The wrong payload length → `RET_INVALID_LEN`. |
| 007e | A `DEST_VALID` of zero → `RET_INVALID_ARG` naming payload word 1. |

### CMD_KEY_TRANSFER — func008

| Sub-test | Description |
|---|---|
| 008a | Load a key, transfer it to HMAC, and recover it from the stub as `SHARE0 XOR SHARE1`. |
| 008b | A destination outside `DEST_VALID` → `RET_FAILURE`. |
| 008c | An invalid handle → `RET_FAILURE`. |
| 008d | The first transfer sets `lock_use`, so a second transfer of the same handle is refused with `RET_FAILURE` and reaches no engine. |
| 008e | The wrong payload length → `RET_INVALID_LEN`, with the received length echoed. |

### CMD_KEY_REVOKE — func009

| Sub-test | Description |
|---|---|
| 009a | Load a key then revoke it: `RET_SUCCESS`, with the handle echoed. |
| 009b | Revoking an already-revoked handle → `RET_FAILURE`. |
| 009c | Too short a payload → `RET_INVALID_LEN`. |

### CMD_ENGINE_SHRED — func010

| Sub-test | Description |
|---|---|
| 010a | Shredding HMAC gives `RET_SUCCESS` and exactly 17 writes to the stub — 8 `SHARE0`, 8 `SHARE1` and `KEY_CTRL`, in a single pass. |
| 010b | Shredding AES and KMAC together reaches both stubs and leaves HMAC and OTBN untouched. |
| 010c | An empty payload → `RET_INVALID_LEN`. |

### Full lifecycle — func011

| Sub-test | Description |
|---|---|
| 011a | Load, transfer to HMAC, revoke; the handle is unusable afterwards. |
| 011b | Generate, transfer to AES, then shred AES. |
| 011c | A multi-command sequence stays in sync, verifying sequence continuity. |

### Reset during operation — func012

| Sub-test | Description |
|---|---|
| 012a | Load a key, assert reset, and confirm the handle no longer resolves — both the registry and the vault are cleared during boot. |
| 012b | After reset the sequence counter restarts, so an old sequence number is rejected. |
| 012c | After reset the outbound FIFO is empty and any pending response is discarded. |
| 012c-extra | A normal command at the next expected sequence number succeeds after reset. |

### Dual-share structure — func013

| Sub-test | Description |
|---|---|
| 013a | Generate then transfer to HMAC, capturing all 17 writes: 8 `SHARE0`, 8 `SHARE1` and `KEY_CTRL` = 1, with `SHARE0 XOR SHARE1` non-trivial overall. Exact values are not checked, since `RAND_bytes` output is not reproducible. |

### Extended coverage — func014

| Sub-test | Description |
|---|---|
| 014a | Emergency wipe through `wipe_ni`, and the unrecoverable-fault and KM flush flow that follows. |
| 014b | Inbound FIFO overflow and its IRQ. |
| 014c | Outbound FIFO overflow. |
| 014d | Outbound FIFO underflow and its IRQ. |
| 014e | A sequence-number error when a prior valid frame has been received. |
| 014f | Vault slot exhaustion. |
| 014g | Payload size and length validation across the commands that take fixed payloads. |
| 014h | The `CMD_RECOV_ACK` flow. |
| 014i | Destination policy enforced at transfer time, including the ABR ports that are accepted and dropped. |
| 014j | `kpv_ctrl_t` serialisation round-trip. |
| 014k | `km_kpv` methods called directly: `km_clear_key`, `km_is_valid`, `km_erase_key`. |
| 014l | Multi-slot key geometry round-trips through the vault — `EXTEND` and `LAST_DWORD` recover the length. |
| 014m | Handles are issued monotonically and never recycled, while revoked slots return to the pool. |

---

## Coverage

### Commands

All fifteen commands are reached. The two retired IDs are pinned to
`RET_INVALID_CMD` in func005, and every return code — `RET_SUCCESS`,
`RET_FAILURE`, `RET_HEADER_CRC`, `RET_CMD_NOSEQ`, `RET_INVALID_CMD`,
`RET_INVALID_LEN`, `RET_PAYLOAD_CRC`, `RET_INVALID_ARG` — is produced somewhere in
the suite.

Note the distinction the suite deliberately pins: commands with a fixed payload
length are rejected by the dispatcher with `RET_INVALID_LEN` before reaching a
handler, whereas `CMD_KEY_LOAD` and `CMD_KEY_GENERATE` validate their own
arguments and report `RET_INVALID_ARG` with the offending word index.

### Registers

All seven mailbox registers are exercised, across every access type present: WO,
RO, RW, W1C and level-sensitive. There are no other SEP-visible registers.

### Vault

Only the KM-firmware path exists, and it is covered: write, read, `lock_write`,
`lock_use`, `erase`, validity, free-slot search, and the multi-slot geometry that
spans up to 8 slots for a 128-word key. The KPVLP path that revision 1.0 covered
has no counterpart, because SEP has no path into the vault.

### Mailbox protocol

Every header and payload error type, separator framing and `MB_WSEP` self-clear,
overflow and underflow with their sticky flags and W1C behaviour, `MB_CTRL.FLUSH`,
and reassembly of frames wider than the 16-word FIFO — exercised by the 17-, 32-
and 128-word key loads in func006.

### Sideload

All four engine sockets are reached, share encoding is verified by reconstruction,
one-shot `lock_use` is enforced, `DEST_VALID` policy is checked including the ABR
destinations, and the OTBN 25-write geometry is covered alongside the 17-write
geometry of the other three.

### Faults, reset and interrupts

Emergency wipe, the unrecoverable-fault flush, `CMD_RECOV_ACK` and the recoverable
status bit are covered in func014 and func003. Reset is covered in func012,
including handle invalidation and sequence restart. Of the five interrupt sources,
`OUTBOUND_READ_DATA_AVAIL`, `OUTBOUND_UNDERFLOW` and `INBOUND_OVERFLOW` are
tested; `FLUSHED_BY_KM` and `INBOUND_WRITE_SPACE_AVAIL` are exercised only
indirectly.

### Pass criteria

A sub-test passes when the response header fields are correct, the response
payload matches the specification, register reads return the expected values,
recording-stub write counts and data match, and IRQ and fault transitions occur as
specified. The suite passes when all 79 sub-tests report no failure and the build
is warning-free. `run_tests.sh` prints `RESULTS: 14 PASSED  0 FAILED`, counting
files rather than sub-tests.

---

## Known Gaps

| Gap | Priority | Proposed test |
|---|---|---|
| Adams Bridge sideload | Medium | Blocked on an ABR model. Today the destinations are accepted and dropped, and `CMD_ABR_SK_TRANSFER` fails; both are pinned, so the tests will need revisiting when ABR arrives. |
| Cold versus warm reset | Medium | The model has one reset input, so `OTP_READ_LOCK_COLD` cannot be shown to survive a warm reset as hardware would. Needs a second reset input first. |
| `FLUSHED_BY_KM` IRQ | Low | Trigger an unrecoverable fault and assert `MB_IRQS.FLUSHED_BY_KM` directly. |
| `INBOUND_WRITE_SPACE_AVAIL` IRQ | Low | Fill the inbound FIFO, enable the bit, drain, and check the level source asserts. |
| Restricted command set during recovery | Medium | With a recoverable fault outstanding, verify commands other than `CMD_RECOV_ACK` are refused — the model accepts them today. |
| Unrecoverable fault halt and recovery | Medium | Force a halt, confirm commands are ignored, then reset and confirm the firmware re-boots. |
| OTP data pass-through | Low | After `set_otp_data()`, confirm `lc_state` and `chiplet_uid` are stored, once a KDF consumes them. |
| Widest key at the FIFO boundary | Low | A 128-word key is covered; the instalment boundaries themselves are not asserted word by word. |

---

## Implementation Notes

### Environment

Each test uses the shared `testbench.h` framework, which provides a
`key_manager_model` instance with its reset and wipe signals, and recording stubs
for the four engine sockets that capture TLM writes and expose the count and data.

### Helpers

Tests build frames through shared helpers in `key_manager_test.h` rather than
hand-rolling CRCs:

- `mb_send_command()` frames and writes a command, polling for inbound space through `mb_wait_inbound_space()` so that frames wider than the FIFO are written in instalments as real firmware would;
- `mb_load_key()` performs a full `CMD_KEY_LOAD` provisioning round-trip and returns the handle — the standard way for a test to obtain a usable key;
- `mb_receive_response()` reads outbound words to the separator and parses them;
- the CRC helpers are exposed so negative tests can corrupt them deliberately.

### Execution order

Files run in numerical order, func001 through func014, each after reset and boot.

---

## Conclusion

The suite covers all fifteen commands, the mailbox protocol including frame
reassembly, mailbox-based provisioning, the key lifecycle, sideload delivery,
fault handling and reset, across 79 sub-tests in 14 files. It documents what is
tested rather than what is desired.

It differs from an RTL test plan in four ways worth restating: no ISS is tested,
since handlers are C++ methods; framing is verified at bit level against the CRC
routines in `rom_crc.c`; share encoding is verified structurally because
`RAND_bytes` output is not reproducible; and DPA countermeasures are excluded to
match the model.

The specification of record for anything this plan asserts is
[`01_key_manager_Specification/`](01_key_manager_Specification/), in particular
`doc/firmware.adoc` and `dv/fw/drivers/rom_cmd.c`.

---

**End of Test Plan**
