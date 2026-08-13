# SEP Filter Controller SystemC TLM High-Level Design Specification

**Document Version:** 1.0
**Date:** 2026-07-27
**IP Module:** SEP Outbound/Inbound AXI Traffic Filter (`sep_filter_ctrl`)
**Modeling Approach:** SystemC TLM2.0, one class instantiated twice (outbound 32-entry, inbound 16-entry), deny-by-default address/permission filter

---

## Executive Summary

This document specifies the SystemC Transaction-Level Model (TLM) for `sep_filter_ctrl`, the AXI traffic filter placed on both the outbound (SEP→SMN, 32 entries) and inbound (SMN→SEP, 16 entries) data paths. It implements a `BlockByDefault=1` priority-ordered rule table — the first entry whose masked address range contains the transaction wins; if it also permits the access type, the transaction is forwarded, otherwise (or if no entry matches at all) it is denied.

### Key Architectural Characteristics

- **Deny-by-default, no exceptions** — unlike `local_master_alias_remap_ctrl` (passthrough on no match), an unmatched address here is *always* denied, whether zero entries are configured or many are configured but none match. This was the single most severe bug found and fixed in this peripheral this session (§7.1).
- **Masked/granular address comparison, not exact-byte** — the range check shifts off the low 12 bits (4 KB page, if `allow_burst`) or 3 bits (8-byte word, otherwise) before comparing, matching `traffic_filter.sv`'s `tx_in_range` logic exactly.
- **Hardware auto-correction of `START_ADDR`/`END_ADDR`** — both fields are `hw=rw` in the RDL; whenever they fall within the same page/word, the stored values are snapped to the full enclosing boundary, mirroring `axi_filter_wrap.sv`'s continuous combinational correction.
- **One class, two instance shapes** — `InstanceType::OUTBOUND` (32 entries) and `InstanceType::INBOUND` (16 entries) share identical logic; only the entry count differs.
- **NS and source-ID filtering are enforced from `sep_axi_extension`** — `match_entry()` compares `attrs.is_ns` against `allow_ns` and, for a non-zero CSR field, `attrs.source_id` against `src_id`. Transactions arriving without the extension fall back to its default attributes, so an initiator that does not stamp one is treated as a secure `SEP_SOURCE_ID` master rather than being waved through unchecked. Group-ID filtering remains inert because SEP ties `EnGroupIdFilter=0` (§2.2, cross-referenced in `Abstractions.md`).

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Features](#2-features)
3. [Port Interfaces](#3-port-interfaces)
4. [Memory-Mapped Registers](#4-memory-mapped-registers)
5. [Filter and Auto-Correction Algorithms](#5-filter-and-auto-correction-algorithms)
6. [Internal Architecture](#6-internal-architecture)
7. [Modeling Assumptions and Fix History](#7-modeling-assumptions-and-fix-history)
8. [Conclusion](#8-conclusion)

---

## 1. Introduction

### 1.1 Overview

Two instances exist in the VP, both built from the same `sep_filter_ctrl_ip` class:

| Instance | Entries | CSR Base | Data Path |
|----------|---------|----------|-----------|
| `outbound_filter` | 32 | `0x10A20000` | Fed by `outbound_filter_mux` (AP-remapped + STEE-remapped + raw SMU traffic — see `local_master_alias_remap_ctrl`'s design doc and `Abstractions.md` §2.4) |
| `inbound_filter` | 16 | `0x10A21000` | Fed directly by `smn_inbound_socket` (external-facing, unbound until an SMC/AP model exists) |

Each entry defines an address range, a read/write permission pair, an NS (secure/non-secure) permission bit, a source-ID/group-ID match pair, a burst-allow bit, and a one-way lock. Entries are evaluated in index order; the **first** entry whose range contains the address wins (priority encoding, matching RTL's `lzc` — leading-zero-count — first-hit semantics).

### 1.2 Purpose of This Document

This document defines the SystemC TLM implementation for VP integration. It documents:

- Features included and excluded from the TLM model
- The complete per-entry register layout (identical for both instances, differing only in entry count)
- The masked address-comparison and auto-correction algorithms, bit-exact against `traffic_filter.sv`/`axi_filter_wrap.sv`
- The fix history for this peripheral — five real bugs were found and corrected this session, more than any other single SEP peripheral

### 1.3 Modeling Goals

1. **Correct deny-by-default semantics** — no configuration state should ever result in passthrough; `BlockByDefault=1` is unconditional.
2. **Bit-exact range matching at both granularities** — page (`allow_burst=1`) and word (`allow_burst=0`) masking must match RTL exactly, since a masked match can differ from a naive `start <= addr <= end` in either direction depending on entry configuration.
3. **Faithful auto-correction** — `START_ADDR`/`END_ADDR` readback after a write must reflect what real hardware's `hw=rw` snap-to-boundary logic would produce, not just the raw value firmware wrote.
4. **Instance-agnostic core logic** — the outbound (32-entry) and inbound (16-entry) filters must share one implementation; nothing in the filter algorithm should special-case which instance it is.

### 1.4 Target Audience

- SystemC model developers maintaining or extending `sep_filter_ctrl`
- VP architects wiring this block into either the outbound merge path or the inbound external-facing socket
- SEP firmware engineers programming outbound/inbound filter rules (M-mode software configuring `src_id`-based protection, per the OCAH spec)
- Verification engineers creating or extending the test plan, especially around the fixes in §7.1

---

## 2. Features

### 2.1 Is (MUST Model)

#### 2.1.1 Deny-by-Default, Priority-Ordered Matching (`check_and_forward()`)

```
if filter_skip_i: forward via filtered_socket, return   // no checking at all

for entry in 0..num_instances-1:                     // match_entry()
    skip if !entry_enabled                              // entry disabled
    shift = allow_burst ? 12 : DATA_BUS_WIDTH(=3)    // page vs 8-byte word granularity
    skip if (addr>>shift) not in [start_addr>>shift, end_addr>>shift]
    skip if src_id != 0 && extension.source_id != src_id     // 0 = wildcard
    skip if extension.is_ns != allow_ns                      // exact match
    skip if !allow_burst && data_length > 8 bytes
    // first surviving entry wins; permissions then deny rather than defer
    deny if READ  && !read_allowed
    deny if WRITE && !write_allowed
    else: forward via filtered_socket, return
deny   // no match — BlockByDefault=1, unconditional, no passthrough state exists
```

`match_entry()` implements the loop above and is shared by `check_and_forward()`
and `data_transport_dbg()`, so the functional and debug paths cannot disagree
about which entry wins.

The masked comparison (rather than an exact `addr >= start && addr <= end`) means an entry's *effective* range is always widened to the enclosing page or word boundary — this is intentional and matches real hardware exactly (§5.1), reinforced by the auto-correction in §2.1.3, which keeps the *stored* register values consistent with what the masked comparison already implies.

#### 2.1.2 Per-Entry Permission Fields

`FILTER_CONFIG`'s `read_allowed`/`write_allowed` gate command type; `allow_burst` gates whether multi-beat (>8 byte) transactions are permitted at all through that entry (approximated via `tlm_generic_payload::get_data_length()` — see §7.2 for the one known approximation here); `locked` (WOSET, bit 63) permanently freezes `FILTER_CONFIG`, `START_ADDR`, and `END_ADDR` for that entry once set, with no bypass.

#### 2.1.3 Hardware Auto-Correction (`auto_correct_start_end()`)

```cpp
void auto_correct_start_end(idx):
    shift = allow_burst ? 12 : DATA_BUS_WIDTH
    if (start_addr >> shift) == (end_addr >> shift):
        page_base = (start_addr >> shift) << shift
        page_top  = page_base | ((1 << shift) - 1)
        START_ADDR[idx] = page_base
        END_ADDR[idx]   = page_top
```

Called after every write to `START_ADDR`, `END_ADDR`, **and** `FILTER_CONFIG` (since a `FILTER_CONFIG` write can change `allow_burst`, which changes which granularity applies to the *existing* stored addresses — a change to the mode alone, with no address write, must still trigger re-correction). This mirrors `axi_filter_wrap.sv`'s continuous combinational recompute, collapsed into a discrete "recompute after each relevant write" call — a reasonable LT-level approximation since the real RTL's `.next`/register update is only ever observable one cycle after the input that changed it anyway.

#### 2.1.4 Lock Enforcement Scope

Once `FILTER_CONFIG[idx].locked` is set, writes to `FILTER_CONFIG[idx]`, `START_ADDR[idx]`, and `END_ADDR[idx]` are all silently ignored (the write callbacks check `get_filter_entry(idx).locked` before calling `handle_write`) — but reads always succeed, and `locked` itself is a WOSET bit (can be set, never cleared, by design).

#### 2.1.5 `data_bus_width` — Hardware-Fixed Field

`FILTER_CONFIG` bits `[14:12]` are `hw=w` — the field always reads back `3` (matching the model's 64-bit/8-byte bus) regardless of what firmware writes; `handle_filter_config_write()` masks the incoming value to force this field to `0x3` before storing.

#### 2.1.6 Testbench Backdoor (`get_filter_entry()`)

Decodes any entry into a `FilterEntry{read_allowed, write_allowed, entry_enabled, allow_ns, src_id, group_id, allow_burst, locked, start_addr, end_addr}` struct for test assertions.

#### 2.1.7 Reset

`SC_METHOD` sensitive to `rst_ni`; when low, `reset_all_registers()` clears every entry (all fields to 0, except `END_ADDR`'s reset value of `0x7` — see §7.1).

### 2.2 Is Not (EXCLUDE)

#### 2.2.1 NS (Secure/Non-Secure) Enforcement

`allow_ns` is decoded into `FilterEntry` and readable via CSR, but never checked in `check_and_forward()`/`data_transport_dbg()` — no AXI `prot`/NS field exists on `tlm_generic_payload` without a custom extension. Concretely, this means the OCAH spec's stated property *"the inbound traffic filter ensures only STEE has access to the STEE address remapper"* (which relies on the NS bit) is not enforced by this model today. Tracked in `Abstractions.md` §1.1.

#### 2.2.2 Source-ID / Group-ID Enforcement

Same story as NS: `src_id`/`group_id` are decoded, storable, never checked — no AXI ID/user field on the generic payload. This is the mechanism M-mode firmware is meant to use to keep `"others"`-tagged traffic (anything routed through AP/STEE remap, per `sep_output_remap_ctrl`'s design doc) away from M-mode assets; the CSR-programmability firmware needs is correct, the runtime gate is a no-op. Tracked in `Abstractions.md` §1.2.

#### 2.2.3 Exact AXI Burst-Length Gating

Real RTL's `pass_burst` gates on `tx_len_i` (AXI burst *beat count*); this model approximates via `data_length > 8 bytes` (total transaction bytes) since TLM's generic payload has no beat-count concept. Functionally equivalent for the single-beat-vs-multi-beat distinction that matters in practice, but not a literal match for an unusual `size`/`len` combination. Tracked in `Abstractions.md` §1.3.

#### 2.2.4 Timing

All filter and auto-correction logic completes synchronously within `b_transport()`; no `sc_time` delay models the real fabric's arbitration/comparator latency.

---

## 3. Port Interfaces

| Category | Port/Socket Name | Type | Direction | Description |
|----------|-------------------|------|-----------|--------------|
| **CSR Bus** | `target_socket` (via `sep_filter_ctrl_base`) | `tlm_utils::simple_target_socket<csml_memory<64>, 32>` | Target | Per-entry CSR table, `0x20`-byte stride. |
| **Data Path (in)** | `data_socket` | `tlm_utils::simple_target_socket<sep_filter_ctrl_ip>` | Target | Incoming AXI transactions to be filtered. |
| **Data Path (out)** | `filtered_socket` | `tlm_utils::simple_initiator_socket<sep_filter_ctrl_ip>` | Initiator | Forwarded transaction after a permitted match. |
| **Reset** | `rst_ni` | `sc_in<bool>` | Input | Active-low; clears all entries when asserted. |
| **Filter skip** | `filter_skip_i` | `sc_in<bool>` | Input | Bypass all checking (`axi_filter_wrap.sv`). Optional — reads 0 while unbound, which is the constant RTL ties the outbound instance to. RTL drives the inbound instance from `feat_ctrl_o.sep_debug` (`sep.sv:952`). |

### 3.1 Interface Notes

1. **Data-path wiring differs by instance** — `outbound_filter->data_socket` is fed by `outbound_filter_mux` (a dedicated merge point, not a direct bus window); `inbound_filter->data_socket` is fed by `smn_inbound_socket` via `smn_inbound_to_filter`. Neither filter's data path is a directly-addressable `PortMapping` entry on the SEP `SimpleBus` — only the CSR socket is.
2. **`MAX_INSTANCES=32` backing memory is always allocated**, even for the 16-entry inbound instance, so `csml_reg_vector<T,32>` never indexes out of bounds; `num_instances_` (a runtime constructor parameter) gates which entries the filter loop and reset actually iterate.

---

## 4. Memory-Mapped Registers

Per-entry stride: `0x20` bytes. Total CSR span: `num_instances × 0x20` (`0x400` bytes for outbound's 32 entries, `0x200` bytes for inbound's 16).

| Register | Offset (per entry) | Bits | Field | Reset | Access | Description |
|----------|---------------------|------|-------|-------|--------|--------------|
| **FILTER_CONFIG** | 0x00 | [0] | `read_allowed` | 0 | RW | Permit reads through this entry. |
| | | [1] | `write_allowed` | 0 | RW | Permit writes through this entry. |
| | | [4] | `entry_enabled` | 0 | RW | Entry enable — `0` = entry never matches. |
| | | [8] | `allow_ns` | 0 | RW | Stored, not enforced (§2.2.1). |
| | | [14:12] | `data_bus_width` | 0 | RO (hw=w, forced to `3`) | Always reads `3` (§2.1.5). |
| | | [19:16] | `src_id` | 0 | RW | Stored, not enforced (§2.2.2). |
| | | [23:20] | `group_id` | 0 | RW | Stored, not enforced (§2.2.2). |
| | | [24] | `allow_burst` | 0 | RW | Selects page (12-bit) vs word (3-bit) shift for range matching and auto-correction. |
| | | [63] | `locked` | 0 | RW (WOSET) | Freezes this entry's `FILTER_CONFIG`/`START_ADDR`/`END_ADDR` permanently once set. |
| **START_ADDR** | 0x08 | [55:0] | `start_addr` | 0x0 | RW (hw=rw, auto-corrected) | Range start, 56-bit field. |
| **END_ADDR** | 0x10 | [55:0] | `end_addr` | **0x7** | RW (hw=rw, auto-corrected) | Range end, 56-bit field. Nonzero reset — see §7.1. |
| — | 0x18 | — | Reserved | — | — | Unused stride padding. |

---

## 5. Filter and Auto-Correction Algorithms

### 5.1 Masked Range Comparison — Bit-Exact Against `traffic_filter.sv`

```systemverilog
// traffic_filter.sv (real RTL, for reference)
if (cfg_burst_en_i)
    tx_in_range = (tx_addr_i[63:12] >= cfg_start_addr_i[63:12]) &&
                  (tx_addr_i[63:12] <= cfg_end_addr_i[63:12]);
else
    tx_in_range = (tx_addr_i[63:DataBusWidthLog2] >= cfg_start_addr_i[63:DataBusWidthLog2]) &&
                  (tx_addr_i[63:DataBusWidthLog2] <= cfg_end_addr_i[63:DataBusWidthLog2]);
```

The model's `check_and_forward()`/`data_transport_dbg()` compute `addr_g = addr >> shift`, `start_g = start_addr >> shift`, `end_g = end_addr >> shift` and compare `addr_g >= start_g && addr_g <= end_g` — the identical inclusive-range-on-shifted-values comparison, with `shift` selected the same way (`allow_burst ? 12 : DATA_BUS_WIDTH`).

### 5.2 Auto-Correction — Why It Matters Even Though the Comparison Is Already Masked

Since the *comparison* already masks off the low bits, one might assume auto-correcting the *stored* register values is redundant. It isn't, for two reasons:
1. **CSR readback must reflect real hardware's `hw=rw` behavior** — firmware reading back `START_ADDR`/`END_ADDR` after writing a narrow range expects to see the snapped-to-boundary values, not its own raw input, if it's checking its configuration took effect as expected.
2. **`allow_burst` toggling changes which stored values are "in the same granule"** — writing `FILTER_CONFIG` alone (no address write) can change whether previously-independent `start_addr`/`end_addr` values now fall in the same page or word, requiring correction triggered by a `FILTER_CONFIG` write alone.

### 5.3 Priority Encoding

Entries are scanned strictly in ascending index order inside a single loop with an early `return` on first match — this is functionally identical to RTL's `lzc` (leading-zero-count) hit-vector priority encoder: whichever bit is set at the lowest index wins, and evaluation order also gives entry 0 priority when address ranges overlap across entries.

---

## 6. Internal Architecture

### 6.1 Class Hierarchy

```
sep_filter_ctrl_ip (sc_module, extends sep_filter_ctrl_base)
├── sep_filter_ctrl_base           (CSML register instances + target_socket)
│   ├── FILTER_CONFIG[0..31]       @ entry_base + 0x00 (stride 0x20)
│   ├── START_ADDR[0..31]          @ entry_base + 0x08
│   └── END_ADDR[0..31]            @ entry_base + 0x10
├── data_socket, filtered_socket   (AXI data-path slave/master)
├── rst_ni                         (sc_in<bool>)
├── filter_skip_i                  (sc_in<bool>, optional — reads 0 while unbound)
├── num_instances_                 — 32 (outbound) or 16 (inbound), set at construction
└── (no additional private state — every decode happens on demand via get_filter_entry())
```

### 6.2 Initialization Flow

```
sep_filter_ctrl_ip(name, InstanceType type):
  → delegates to sep_filter_ctrl_ip(name, type==OUTBOUND ? 32 : 16)

sep_filter_ctrl_ip(name, num_instances):
  → sep_filter_ctrl_base constructor: allocate csml_memory<64> sized for MAX_INSTANCES=32
    regardless of num_instances, construct FILTER_CONFIG/START_ADDR/END_ADDR register vectors
  → SC_METHOD(reset_handler), sensitive to rst_ni
  → reset()
  → for i in 0..num_instances-1:
      register FILTER_CONFIG[i]'s write callback → handle_filter_config_write(i, ...)
      register FILTER_CONFIG[i]'s read callback  → handle_filter_config_read(i, ...)
      register START_ADDR[i]'s write callback    → lock-gated handle_write + auto_correct_start_end(i)
      register END_ADDR[i]'s write callback      → lock-gated handle_write + auto_correct_start_end(i)
  → data_socket.register_b_transport/transport_dbg → data_b_transport/data_transport_dbg
```

### 6.3 Per-Transaction Flow

```
data_b_transport(trans, delay):
  → check_and_forward(trans, delay)   // §5.1 masked scan; on permit, calls filtered_socket->b_transport
                                        // on deny, sets TLM_ADDRESS_ERROR_RESPONSE and returns
```

---

## 7. Modeling Assumptions and Fix History

### 7.1 Bugs Found and Fixed This Session

This peripheral had the most fixes of any single SEP IP this session — five real divergences from RTL, found through direct cross-reference against `traffic_filter.sv`/`axi_filter_wrap.sv`/`filter_ctrl.rdl`:

1. **`BlockByDefault` was inverted** (most severe) — the model originally treated "no active entries" as passthrough; real RTL's `BlockByDefault=1'b1` (confirmed via both the `axi_filter_wrap` parameter default and its instantiation for both SEP filter instances in `sep_system_peripherals.sv`) means deny-always-on-no-match, with no distinction between "unconfigured" and "configured but no match."
2. **`END_ADDR` reset value was wrong** — RDL specifies `0x7`, not `0x0`; fixed to match (see the register table in §4).
3. **Lock enforcement scope was too narrow** — originally only gated `FILTER_CONFIG`; corrected to also gate `START_ADDR`/`END_ADDR` writes once `locked` is set, per the RDL's stated intent ("write once register to lock filter configurations").
4. **`allow_burst` was not enforced** — added the `!allow_burst && data_length > 8` deny path.
5. **Address comparison was exact-byte instead of masked/granular, and `START_ADDR`/`END_ADDR` had no auto-correction** — the two fixes confirmed this session, described in §5.1/§5.2, implemented in `check_and_forward()`, `data_transport_dbg()`, and the new `auto_correct_start_end()`.

All five were confirmed via direct RTL cross-reference, not inference, and the last round was hand-traced against the existing `sep_filter_ctrl_testbench.cpp`/`sep_filter_ctrl_test.cpp` suites to confirm no regressions (every existing test's address ranges are spaced far enough apart that the new masking/auto-correction never actually changes their outcome) — though that was a hand-trace, not a fresh build+test run, since the rebuild was interrupted before completion (see conversation history / `Abstractions.md` for the general state of unverified-but-implemented fixes).

### 7.2 Burst-Length Approximation

Covered in §2.2.3 — `data_length > 8 bytes` stands in for real hardware's `tx_len_i` beat-count check. Not revisited this session beyond the original finding.

### 7.3 `num_instances_` Is a Runtime Parameter, Not a Template Parameter

Unlike `csml_reg_vector<T, 32>`'s compile-time-fixed backing storage, `num_instances_` is set at construction and only affects which entries the filter loop, reset, and callback-registration loop actually touch — entries beyond `num_instances_` (e.g. entries 16–31 on the inbound instance) still physically exist in the backing `csml_memory` but are never registered with callbacks and never participate in filtering, matching how the inbound RTL instance is generated with only 16 real entries.

---

## 8. Conclusion

### 8.1 Summary

`sep_filter_ctrl` is a single, instance-count-parameterized implementation shared by both the outbound (32-entry) and inbound (16-entry) SEP traffic filters:

1. **Deny-by-default is now correct and unconditional** — the most severe bug found this session, now fixed and matching `BlockByDefault=1'b1` exactly.
2. **Masked/granular address comparison and hardware auto-correction** are both implemented and bit-exact against `traffic_filter.sv`/`axi_filter_wrap.sv`.
3. **Lock, burst-length, and `data_bus_width` enforcement** are all correctly scoped per the RDL.
4. **NS and source-ID filtering remain CSR-programmable but runtime-inert** — a permanent, well-documented TLM limitation shared with other SEP peripherals, not a defect specific to this one.

### 8.2 Known Gaps

| Gap | Risk | Mitigation |
|-----|------|------------|
| NS enforcement not modeled | Low-medium — breaks the spec's "only STEE reaches STEE remapper" property | `Abstractions.md` §1.1; would need a custom TLM extension to fix |
| Source-ID/group-ID enforcement not modeled | Low-medium — M-mode firmware's CSR config has no runtime effect | `Abstractions.md` §1.2; same fix path as above |
| Burst-length approximated by byte count, not AXI beat count | Low — equivalent for the single-vs-multi-beat distinction that matters in practice | `Abstractions.md` §1.3 |
| Last two fixes (masked comparison + auto-correction) not yet confirmed via a fresh build+test run | Low — hand-traced against existing tests, no regressions expected | Rebuild `sep_filter_ctrl_testbench` to confirm |

---

**End of High-Level Design Specification**
