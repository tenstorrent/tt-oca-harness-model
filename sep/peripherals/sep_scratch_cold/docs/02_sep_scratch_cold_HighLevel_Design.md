# SEP Scratch Cold Register Bank SystemC TLM High-Level Design Specification

**Document Version:** 1.0
**Date:** 2026-07-27
**IP Module:** SEP Scratch Cold Register Bank (`sep_scratch_cold`)
**Modeling Approach:** SystemC TLM2.0, plain 8×64-bit storage with post-write decode taps

---

## Executive Summary

At the register level, `sep_scratch_cold` really is just 8 plain read/write 64-bit words — but the storage itself is the least interesting part of this peripheral. Two of the eight slots are tapped by pure (SystemC-free) decoder classes that turn raw firmware writes into human-readable simulation output: `SCRATCH[2]` decodes the bootcode "virtual console" protocol (`virt_console_decoder.h`), and `SCRATCH[1]` decodes the production status-reporting protocol (`sep_status_decoder.h`). Three more slots are tapped by VP-only handshake shims that unblock specific firmware tests originally written for a CocoTB-based DV testbench.

### Key Architectural Characteristics

- **Storage is trivial; behavior is not** — the CSML register layer is 8 identical plain words; all the actual logic lives in `register_post_write_callback()` taps installed in the constructor, not in the register definitions themselves.
- **Decoders are deliberately SystemC/CSML-free** — both `VirtConsoleDecoder` and `StatusDecoder` take only a `uint32_t` word and an `EmitFn` callback, so they're unit-testable in isolation and, per their own header comments, mirror a corresponding cocotb reference decoder exactly (so VP and RTL DV runs report identical messages).
- **`StatusDecoder`'s name table is now loaded** — `[SEP_STATUS]` lines resolve real `SEP_MSG_*` names via a vendored snapshot of the firmware's `status_values.h` (`docs/01_sep_scratch_cold_Specification/status_values.h`), parsed at construction and loaded via `set_names()`. This was found unwired earlier and has since been fixed (§2.1.6, §7.1).
- **Three VP-only CocoTB-handshake shims** exist purely to keep specific firmware tests from hanging forever in a standalone VP run — not modeling any real hardware behavior at all (§2.1.4).

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Features](#2-features)
3. [Port Interfaces](#3-port-interfaces)
4. [Memory-Mapped Registers](#4-memory-mapped-registers)
5. [Decoder Protocols](#5-decoder-protocols)
6. [Internal Architecture](#6-internal-architecture)
7. [Modeling Assumptions](#7-modeling-assumptions)
8. [Conclusion](#8-conclusion)

---

## 1. Introduction

Firmware uses `SEP_SCRATCH_COLD_SCRATCH_0..7` (8 general-purpose 64-bit words, `0x1080_2000`–`0x1080_203F` per `Args.hpp`) for a mix of genuine scratch storage and, on this VP, three purposes with real observable behavior:

1. **Boot-time console logging** (`SCRATCH[2]`) — the bootcode's lightweight `simput*` helpers, used before any UART is available.
2. **Production status reporting** (`SCRATCH[1]`) — the same word firmware unconditionally writes via `STATUS_OUT` (`errors.h`) before pushing to the SMC status ring buffer; tapping it here captures every message *including* `DEBUG`-type ones the ring buffer itself drops.
3. **VP-only test synchronization** (`SCRATCH[0]`/`[1]`, `SCRATCH[4]`/`[5]`, `SCRATCH[6]`/`[7]`) — simulated CocoTB acks for three specific firmware tests.

**Target audience:** SystemC model developers, VP integrators who want simulation console output, firmware engineers relying on `simput`/`STATUS_OUT` logging, verification engineers extending the affected test list.

---

## 2. Features

### 2.1 Is (MUST Model)

#### 2.1.1 Plain 8×64-bit Scratch Storage

All eight `SCRATCH[0..7]` words are ordinary CSML read/write storage — no field structure, no locks, reset to 0.

#### 2.1.2 Virtual Console Decode (`SCRATCH[2]`)

Every write to `SCRATCH[2]` fires a `post_write_callback` that feeds the raw 32-bit low word to `VirtConsoleDecoder::on_word()`, printed as `[SIM_OUT] <line>`. Enabled/disabled via the `sim_out.enable` CCI param (default `true`).

#### 2.1.3 Production Status Decode (`SCRATCH[1]`)

Every write to `SCRATCH[1]` fires a callback feeding the word to `StatusDecoder::on_word()`, printed as `[SEP_STATUS] <line>`. Enabled/disabled via `sep_status.enable` (default `true`).

#### 2.1.4 VP-Only CocoTB-Handshake Shims

Three independent `post_write_callback`s simulate what a real CocoTB DV testbench would do, so specific firmware tests don't hang in a standalone VP run (no CocoTB process exists to answer them):

| Test | Firmware writes | To | VP responds | To |
|------|------------------|-----|--------------|-----|
| `global_alias_remap_sanity` | `0x12345678` | `SCRATCH[0]` | `0x87654321` | `SCRATCH[1]` |
| `ap_stee_output_remap_test` | `0x815` | `SCRATCH[4]` | `0x777` | `SCRATCH[5]` |
| `sep_aes_large_payload_test` (STRESS-002) | `0xA1E50006` (`FW_READY_MAGIC`) | `SCRATCH[6]` | `0x00100001` (16 blocks, seed=1) | `SCRATCH[7]` |

These are pure VP scaffolding — they exist only because these three tests' firmware source expects a CocoTB partner to be listening, and this VP has none.

#### 2.1.5 Reset

`reset_all_registers()` clears all 8 words to 0 at construction (there is no `rst_ni` port on this peripheral — reset only happens once, at construction, not on any signal edge).

#### 2.1.6 Status Name Table — Loaded from a Vendored Snapshot

`StatusDecoder::set_names()` installs a `value → symbolic name` map. Rather than reading the firmware's canonical `meta/status/status_values.tsv` live from `sw/tt-oca-hw-main` (a cross-repo, working-directory-relative path — fragile, and inconsistent with how every other peripheral this session vendors its reference material locally), this model instead:

1. Vendors a snapshot of `status_values.h` (the same data, already generated by firmware's own build into plain `#define SEP_MSG_XXX 0xYY` lines) into `docs/01_sep_scratch_cold_Specification/status_values.h`.
2. Parses it with `StatusDecoder::parse_tsv()` — extended to recognize an optional leading `#define` token, so the same parser transparently handles both the canonical TSV's `NAME 0xVALUE` rows and the vendored header's `#define NAME 0xVALUE` rows, with no duplicated comment-skipping logic.
3. Resolves its path via a CMake-baked absolute macro (`SEP_SCRATCH_COLD_STATUS_VALUES_PATH`, set in `CMakeLists.txt` from `CMAKE_CURRENT_SOURCE_DIR`), not a runtime-relative path — so name loading never depends on the process's working directory or on `sw/tt-oca-hw-main` being checked out at any particular relative location.

`[SEP_STATUS]` lines now show real `SEP_MSG_*` names instead of `SEP_MSG_UNKNOWN`. See §7.1 for the tradeoff this vendoring choice makes.

### 2.2 Is Not (EXCLUDE)

#### 2.2.1 No Reset Port

Unlike every other SEP peripheral documented this session, `sep_scratch_cold_ip` has no `rst_ni` input — registers are cleared once, in the constructor, and never again. A firmware test that expects scratch content to survive a warm reset (or be cleared by one) would see VP behavior that depends entirely on whether the simulation restarts the module, not on any modeled reset signal.

#### 2.2.2 Timing

Decoder invocation happens synchronously inside the same `b_transport()` that stores the write; no delay models any real logging/status-reporting latency.

---

## 3. Port Interfaces

| Category | Port/Socket Name | Type | Direction | Description |
|----------|-------------------|------|-----------|--------------|
| **Register Bus** | `target_socket` (via `sep_scratch_cold_base`) | `tlm_utils::simple_target_socket<csml_memory<64>, 32>` | Target | 8×64-bit window, `0x40` bytes. |

No reset port, no data-path sockets, no other I/O — this is the only peripheral documented this session with just one interface.

---

## 4. Memory-Mapped Registers

8-byte stride, 8 entries, `0x40` bytes total.

| Register | Offset | Reset | Access | Special Behavior |
|----------|--------|-------|--------|--------------------|
| **SCRATCH[0]** | 0x00 | 0 | RW | Tapped: writing `0x12345678` triggers the `global_alias_remap_sanity` ack (§2.1.4). |
| **SCRATCH[1]** | 0x08 | 0 | RW | Tapped: every write decoded as a production status word → `[SEP_STATUS]` (§2.1.3). Also the ack target for `SCRATCH[0]`'s shim. |
| **SCRATCH[2]** | 0x10 | 0 | RW | Tapped: every write decoded as a virtual-console word → `[SIM_OUT]` (§2.1.2). |
| **SCRATCH[3]** | 0x18 | 0 | RW | Plain storage, no tap. |
| **SCRATCH[4]** | 0x20 | 0 | RW | Tapped: writing `0x815` triggers the `ap_stee_output_remap_test` ack (§2.1.4). |
| **SCRATCH[5]** | 0x28 | 0 | RW | Plain storage; ack target for `SCRATCH[4]`'s shim. |
| **SCRATCH[6]** | 0x30 | 0 | RW | Tapped: writing `0xA1E50006` triggers the `sep_aes_large_payload_test` ack (§2.1.4). |
| **SCRATCH[7]** | 0x38 | 0 | RW | Plain storage; ack target for `SCRATCH[6]`'s shim. |

---

## 5. Decoder Protocols

### 5.1 Virtual Console Word Format (`rom_virt_console.h`)

```
[31:8] payload   [7:4] reserved   [3:1] opcode   [0] toggle (masked out, not part of payload)
opcode 0 (ASCII): up to 3 bytes at [15:8]/[23:16]/[31:24], NUL-terminated, line-buffered on '\n'
opcode 1 (HEX16): 16-bit value at [23:8], rendered as 4 lowercase hex digits
opcode 2 (DEC24): 24-bit value at [31:8], rendered as decimal
```
Unknown opcodes are silently ignored (never abort). `VirtConsoleDecoder` line-buffers ASCII fragments so partial multi-word sequences print as one complete line.

### 5.2 Production Status Word Format (`errors.h`)

```
[31:24] type   [23:16] firmware-id (fw_id)   [15:0] value
```
Rendered as `"<stage> <SEVERITY> 0x%04x <NAME>"`, e.g. `BL0 INFO     0x0044 SEP_MSG_BOOTROM_START`. `type` maps `INFO(0x01)/WARN(0x08)/ERROR(0x0f)/INFO_EXT(0x81)` to labels (anything else renders as `T0x%02x`); `fw_id` maps `1→BL0`, `2→BL1` (else `ID%u`); `DEBUG (0x80)` is documented as never reaching this path at all (firmware skips the ring for DEBUG-type messages, though the type label itself is still implemented for completeness). `name` is a lookup resolved from the vendored `status_values.h` snapshot loaded at construction (§2.1.6); an unrecognized value still renders `SEP_MSG_UNKNOWN`.

---

## 6. Internal Architecture

```
sep_scratch_cold_ip (sc_module, extends sep_scratch_cold_base)
├── sep_scratch_cold_base          (SCRATCH[0..7] + target_socket)
├── csml_param<bool> sim_out_enable, sep_status_enable
├── csml_param<int>  verbosity
└── (private)
    ├── sep_virt_console::VirtConsoleDecoder vconsole_decoder_   — tapped on SCRATCH[2]
    └── sep_status_report::StatusDecoder     status_decoder_    — tapped on SCRATCH[1]
```

Both decoder classes live in their own headers with zero SystemC/CSML dependency — `on_word(uint32_t)`/`on_bytes(const uint8_t*, unsigned)` and an `EmitFn` callback are their entire public surface for decoding, making them directly unit-testable without instantiating any SystemC module.

### 6.1 Initialization Flow

```
sep_scratch_cold_ip constructor:
  → sep_scratch_cold_base constructor: allocate csml_memory<64> (64 B / 8 = 8 words), construct SCRATCH[0..7]
  → reset_all_registers()
  → vconsole_decoder_.set_enabled(sim_out_enable);      vconsole_decoder_.set_emit(print "[SIM_OUT] ...")
  → status_decoder_.set_enabled(sep_status_enable);     status_decoder_.set_emit(print "[SEP_STATUS] ...")
  → register 5 post_write_callbacks: SCRATCH[2] (console), SCRATCH[1] (status),
    SCRATCH[0]/[4]/[6] (CocoTB-ack shims)
```

---

## 7. Modeling Assumptions

### 7.1 Vendored Snapshot vs. Live Cross-Repo Read — a Deliberate Tradeoff

The name table is loaded from a vendored copy of `status_values.h` (§2.1.6) rather than reading the canonical `meta/status/status_values.tsv` live from `sw/tt-oca-hw-main` at runtime. This trades automatic freshness for independence: if firmware's TSV gains new `SEP_MSG_*` codes later, this peripheral's vendored copy won't see them until someone manually re-copies `status_values.h` — but the model never depends on `sw/tt-oca-hw-main` being checked out at a specific relative location, or on the process's working directory, to find its reference data. This mirrors the same vendoring choice already made for every other peripheral's `docs/01_..._Specification/` folder this session.

### 7.2 No Reset Port Is a Deliberate Simplification, Not an Oversight

Scratch content in real hardware is presumably reset alongside the rest of SEP on a cold/warm reset; this VP only clears it once, at construction. Given none of this peripheral's three real behaviors (console decode, status decode, CocoTB shims) depend on stale scratch content surviving across a simulated reset in any of the current tests, this has no observed practical impact today.

### 7.3 CocoTB Shims Are Address- and Value-Specific, Not General

Each shim matches one exact magic value at one exact offset — there is no generic "any CocoTB-style handshake" mechanism here. Adding support for a fourth test with a different handshake protocol would mean adding a fourth bespoke callback, not extending a shared mechanism.

---

## 8. Conclusion

### 8.1 Summary

`sep_scratch_cold` is a minimal register bank whose entire value lies in three post-write taps: two genuine boot-log/status decoders (SystemC-free, cocotb-mirroring, directly reusable/testable in isolation, and now name-resolving via a vendored snapshot) and three narrow VP-only test-unblocking shims.

### 8.2 Known Gaps

| Gap | Risk | Mitigation |
|-----|------|------------|
| Status name table is a vendored snapshot, not a live read of firmware's canonical TSV | Low — matches the vendoring pattern already used for every other peripheral's spec material this session | Manually re-copy `status_values.h` if firmware adds new `SEP_MSG_*` codes and names go stale |
| No reset port | Low — no current test depends on scratch content surviving or being cleared by a simulated reset | N/A unless a future test needs it |
| CocoTB shims are per-test, not general | Low — matches the narrow scope of what's needed today | Add a new bespoke callback if a new test needs the same style of handshake |

---

**End of High-Level Design Specification**
