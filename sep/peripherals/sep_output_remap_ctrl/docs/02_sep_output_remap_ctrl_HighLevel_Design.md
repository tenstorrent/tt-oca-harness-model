# SEP Output Remap Controller SystemC TLM High-Level Design Specification

**Document Version:** 1.0
**Date:** 2026-07-27
**IP Module:** SEP AP/STEE Output Remap Controller (`sep_output_remap_ctrl`)
**Modeling Approach:** SystemC TLM2.0, one class instantiated twice (AP, STEE), 16-region index-replace address remapper

---

## Executive Summary

This document specifies the SystemC Transaction-Level Model (TLM) for `sep_output_remap_ctrl`, the address remapper on SEP's outbound-to-Chiplet path. Two instances exist — `ap_output_remap` and `stee_output_remap` — each independently controlled by an external agent (Ascalon/AP or STEE) that programs its 16-entry table. Unlike `local_master_alias_remap_ctrl`'s additive, range-checked remap, this peripheral **replaces** the upper address bits with a table-indexed value; every address in its fixed input window always indexes something, so there is no "no match" state to handle.

### Key Architectural Characteristics

- **Index-replace, not range-check-and-add** — the region index is extracted directly from address bits (`adjusted[IdxStart+RemapIndexW-1:IdxStart]`), not compared against a `[start,end)` range. This is the single most important distinction from `local_master_alias_remap_ctrl`, despite both peripherals being called "remap" controllers.
- **This was the most-verified peripheral of the four core SEP remap/filter IPs this session** — independently re-audited against the real silicon RTL tree (`sep_pkg.sv`, `sep_system_peripherals.sv`, `sep_system_csr.sv`, `och_sep_top_reg.svh`), not just the bundled `rtl-details`/specification snapshot, with zero new bugs found.
- **Two hardware-fixed instance configurations** — `AP_REGION_BASE=0x11000000`/`AP_NUM_REGIONS=16`/`AP_IDX_START=19` and `STEE_REGION_BASE=0x11800000`/`STEE_NUM_REGIONS=16`/`STEE_IDX_START=19`, both giving 512 KB per-region granularity, confirmed bit-exact against `sep_pkg.sv`'s real constants.
- **Two permanent, documented TLM gaps** — AXI user-field retagging and CSR byte-enable — both shared limitations with other SEP peripherals, not defects specific to this one.

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Features](#2-features)
3. [Port Interfaces](#3-port-interfaces)
4. [Memory-Mapped Registers](#4-memory-mapped-registers)
5. [Remap Algorithm](#5-remap-algorithm)
6. [Internal Architecture](#6-internal-architecture)
7. [Modeling Assumptions](#7-modeling-assumptions)
8. [Conclusion](#8-conclusion)

---

## 1. Introduction

### 1.1 Overview

Two instances, same class, different hardware constants:

| Instance | Region Base | Regions | Idx Start | Granularity | Controlled By |
|----------|-------------|---------|-----------|-------------|----------------|
| `ap_output_remap` | `0x11000000` | 16 | 19 | 512 KB | AP (Ascalon) |
| `stee_output_remap` | `0x11800000` | 16 | 19 | 512 KB | STEE |

Both feed into `outbound_filter_mux` (see `sep_filter_ctrl`'s design doc §1.1 and `Abstractions.md` §2.4), converging with the raw SMU leg before the shared outbound filter.

### 1.2 Distinguishing This IP from `local_master_alias_remap_ctrl`

Both are called "remap controllers" and both operate on a 16-entry table, but the algorithms are opposite in structure:

| | `local_master_alias_remap_ctrl` | `sep_output_remap_ctrl` (this IP) |
|---|---|---|
| Match | Explicit `[start, end)` range check per region | Direct index extraction from address bits — always "matches" |
| Operation | **Add** offset to upper address bits | **Replace** upper address bits with table entry |
| No-match | Passthrough unchanged | Never occurs (every address in the fixed input window indexes something) |
| Granularity | 4 KB page (`IdxStart=12`) | 512 KB (`IdxStart=19`) |
| Table size | 16, shared address space with CSRs | 16 per instance, ×2 instances |

### 1.3 Purpose of This Document

This document defines the SystemC TLM implementation for VP integration. It documents:

- The index-replace algorithm, bit-exact against `output_remap.sv`
- The two hardware-fixed instance configurations (AP, STEE)
- The two permanent TLM gaps already identified and documented as code comments
- Cross-references confirming this peripheral's re-audit against the real silicon RTL tree, independent of the bundled specification snapshot

### 1.4 Modeling Goals

1. **Bit-exact index extraction and replacement** — for any firmware-programmed 16-entry table, the remapped address must match real RTL exactly, including the `% NUM_REGIONS` safety fallback for a non-power-of-2 guard (redundant but harmless for the always-power-of-2 region counts actually used).
2. **Correct dual-instantiation independence** — the AP and STEE instances must never share state; a CSR write to one must have zero effect on the other.
3. **Faithful lower-bits preservation** — bits below `IdxStart` must always pass through unchanged from the original address, regardless of table content.

### 1.5 Target Audience

- SystemC model developers maintaining or extending `sep_output_remap_ctrl`
- VP architects wiring AP/STEE remap into the outbound merge path
- Firmware/external-agent engineers (AP/Ascalon, STEE) programming the 16-entry tables
- Verification engineers extending the test plan

---

## 2. Features

### 2.1 Is (MUST Model)

#### 2.1.1 Index-Replace Remap (`remap_address()`)

```cpp
adjusted    = addr - REGION_BASE
idx         = ((adjusted & idx_mask_) >> IDX_START) % NUM_REGIONS   // extraction + power-of-2 guard
offset      = REGION_ATTRS[idx].offset & 0x00FFFFFFFFFFFFFF          // 56-bit field mask
upper_bits  = offset & ~lower_mask_
lower_bits  = adjusted & lower_mask_
return upper_bits | lower_bits
```

`lower_mask_ = (1 << IDX_START) - 1` (bits below the index field, preserved verbatim) and `idx_mask_ = ((1 << idx_width_) - 1) << IDX_START` (exactly the index-field bits, nothing more) are both derived once at construction from `IDX_START` and `log2(NUM_REGIONS)`. The `% NUM_REGIONS` is a defensive no-op for any power-of-2 region count (enforced by a constructor check — see §2.1.4), matching the real RTL's plain bit-slice (`adjusted_addr[IdxStart+:RemapIndexW]`), which inherently ranges over exactly `NUM_REGIONS` values with no wraparound needed.

#### 2.1.2 Address Restore After Forward

`data_b_transport()`/`data_transport_dbg()` save the original address, overwrite it with the remapped result to forward, then restore the original address on the payload afterward.

#### 2.1.3 Dual Constructors

```cpp
sep_output_remap_ctrl_ip(name, InstanceType::AP)     // resolves AP_REGION_BASE/AP_NUM_REGIONS/AP_IDX_START internally
sep_output_remap_ctrl_ip(name, region_base, num_regions=16, idx_start=19)   // explicit, for testbenches
```

Both delegate to the same underlying parametric constructor; there is no behavioral difference between the two AP/STEE instances beyond the three hardware constants passed in.

#### 2.1.4 Construction-Time Validation

The constructor `SC_REPORT_FATAL`s if `num_regions` is zero or not a power of 2, or if it exceeds `MAX_REGIONS=16` — guarding the `% NUM_REGIONS` safety net in §2.1.1 and the fixed-size `REGION_ATTRS` backing array.

#### 2.1.5 Reset

`SC_METHOD` sensitive to `rst_ni`; when low, `reset_all_registers()` clears all 16 `REGION_ATTRS` entries to `0`.

### 2.2 Is Not (EXCLUDE)

#### 2.2.1 AXI User-Field Retagging

Real `output_remap.sv` forces `aw.user`/`w.user`/`ar.user` to a fixed `UserOverrideVal` (`OTHERS_SOURCE_ID`) on every remapped transaction, via the `prim_axi_user_override_struct` submodule (gated by `UserOverrideEn`, `1'b1` for both SEP instances). No AXI `user` field exists on `tlm_generic_payload`, and nothing downstream in the VP currently inspects a user/source-ID sideband to make a functional decision — the same underlying limitation as `sep_filter_ctrl`'s `src_id`/`group_id` gap. Tracked in `Abstractions.md` §1.4.

#### 2.2.2 CSR Byte-Enable

The RDL declares `accesswidth=64`, and real hardware's generated regblock additionally merges partial (sub-64-bit) writes using `wstrb`-derived bit-enables. The underlying `csml_memory`/`csml_reg` write path always applies a full 64-bit overwrite regardless of the TLM byte-enable mask. Shared limitation of the `csml` register library, not specific to this peripheral; no firmware access pattern found (`ap_stee_output_remap_test.c`) uses anything but full 64-bit reads/writes, so no observed practical impact. Tracked in `Abstractions.md` §1.5.

#### 2.2.3 `cacheable` Attribute

Unlike `local_master_alias_remap_ctrl`'s `REGION_ATTRS.cacheable` bit, this peripheral's RDL has **no** cacheable field at all (`offset[55:0]` is the only field) — confirmed directly against the real RTL's register package, not merely assumed by analogy to the other remap IP. Nothing to exclude here; it's simply absent from the real hardware too.

#### 2.2.4 Timing

All remap and CSR operations complete synchronously within `b_transport()`; no `sc_time` delay models the real fabric's index-lookup/mux latency.

---

## 3. Port Interfaces

| Category | Port/Socket Name | Type | Direction | Description |
|----------|-------------------|------|-----------|--------------|
| **CSR Bus** | `target_socket` (via `sep_output_remap_ctrl_base`) | `tlm_utils::simple_target_socket<csml_memory<64>, 32>` | Target | 16-entry table, 8-byte stride, 128-byte total window. |
| **Data Path (in)** | `data_socket` | `tlm_utils::simple_target_socket<sep_output_remap_ctrl_ip>` | Target | Incoming AXI transactions bound for this instance's fixed address window. |
| **Data Path (out)** | `remapped_socket` | `tlm_utils::simple_initiator_socket<sep_output_remap_ctrl_ip>` | Initiator | Remapped transaction, forwarded toward `outbound_filter_mux`. |
| **Reset** | `rst_ni` | `sc_in<bool>` | Input | Active-low; clears all 16 `REGION_ATTRS` entries when asserted. |

### 3.1 Interface Notes

1. **`data_socket`'s address window is reached via the SEP `SimpleBus`'s static `PortMapping`** at `ap_remap_data_start_addr`/`stee_remap_data_start_addr` in `Args.hpp` (`0x11000000`/`0x11800000`, matching the hardware-fixed `AP_REGION_BASE`/`STEE_REGION_BASE` constants exactly — the bus routing and the model's own internal constants agree by construction, not by coincidence).
2. **`remapped_socket` is not a bus target** — it binds directly to `outbound_filter_mux`'s `ap_tgt`/`stee_tgt` sockets (a point-to-point connection), not back onto the `SimpleBus`, mirroring `sep_system_peripherals.sv`'s `u_outbound_filter_mux` direct wiring.

---

## 4. Memory-Mapped Registers

16 entries, 8-byte stride, 128 bytes total (`MEMORY_SIZE = MAX_REGIONS * 8`).

| Register | Offset (per entry) | Bits | Field | Reset | Access | Description |
|----------|---------------------|------|-------|-------|--------|--------------|
| **REGION_ATTRS** | entry × 0x8 | [55:0] | `offset` | 0x0 | RW | Replacement value for the address's upper bits (56-bit field; only the top `56 - IdxStart = 37` bits are actually used after the lower-bits mask is applied, but the full field is CSR-visible). |
| | | [63:56] | Reserved | 0x0 | — | Always 0. |

---

## 5. Remap Algorithm

### 5.1 Worked Example (AP Instance)

`AP_REGION_BASE=0x11000000`, `AP_IDX_START=19` (512 KB granularity), `AP_NUM_REGIONS=16` → index field is bits `[22:19]` of the base-adjusted address.

Incoming address `0x11234567`:
- `adjusted = 0x11234567 - 0x11000000 = 0x00234567`
- `idx = (0x00234567 & idx_mask_) >> 19` — bits `[22:19]` of `0x00234567` → `idx = 4` (since `0x234567 >> 19 = 4`, verify: `4 << 19 = 0x200000`, and `0x234567` is indeed in `[0x200000, 0x280000)`)
- If `REGION_ATTRS[4].offset = 0x0040000000000`, then: `upper_bits = 0x0040000000000 & ~0x7FFFF`, `lower_bits = 0x00234567 & 0x7FFFF = 0x34567`
- Result: `upper_bits | 0x34567`

### 5.2 Why the Real RTL Bit-Slice Never Needs a Modulo

Verilog's `adjusted_addr[IdxStart+:RemapIndexW]` extracts exactly `RemapIndexW = log2(NumRegions)` bits — for a power-of-2 region count, this slice inherently produces a value in `[0, NumRegions)` with no possibility of exceeding the table size. The model's `% NUM_REGIONS` is a defensive no-op preserved for clarity/safety, not a behavioral difference — it can only ever matter if `idx_mask_`'s bit width were ever miscalculated relative to `NUM_REGIONS`, which the constructor's power-of-2 check (§2.1.4) prevents from happening in the first place.

---

## 6. Internal Architecture

### 6.1 Class Hierarchy

```
sep_output_remap_ctrl_ip (sc_module, extends sep_output_remap_ctrl_base)
├── sep_output_remap_ctrl_base      (CSML register instances + target_socket)
│   └── REGION_ATTRS[0..15]         @ entry × 0x8
├── data_socket, remapped_socket    (AXI data-path slave/master)
├── rst_ni                          (sc_in<bool>)
├── const REGION_BASE, NUM_REGIONS, IDX_START   — set once at construction, per instance
└── (private)
    ├── uint32_t idx_width_    — log2(NUM_REGIONS), computed at construction
    ├── uint64_t lower_mask_   — bits [IDX_START-1:0]
    └── uint64_t idx_mask_     — bits [IDX_START+idx_width_-1:IDX_START]
```

### 6.2 Initialization Flow

```
sep_output_remap_ctrl_ip(name, InstanceType type):
  → delegates to the parametric constructor with AP_* or STEE_* constants

sep_output_remap_ctrl_ip(name, region_base, num_regions, idx_start):
  → sep_output_remap_ctrl_base constructor: allocate csml_memory<64> (128 B / 8 = 16 words),
    construct REGION_ATTRS[0..15]
  → SC_METHOD(reset_handler), sensitive to rst_ni
  → validate num_regions (power-of-2, <= MAX_REGIONS) — SC_REPORT_FATAL otherwise
  → derive idx_width_ = log2(num_regions)
  → derive lower_mask_, idx_mask_ from idx_start/idx_width_
  → data_socket.register_b_transport/transport_dbg
```

### 6.3 Per-Transaction Flow

```
data_b_transport(trans, delay):
  orig_addr = trans.get_address()
  new_addr  = remap_address(orig_addr)   // §5.1
  trans.set_address(new_addr)
  remapped_socket->b_transport(trans, delay)
  trans.set_address(orig_addr)           // restore
```

---

## 7. Modeling Assumptions

### 7.1 Independent Re-Audit, Not Just the Bundled Specification

This peripheral was the only one of the four core SEP remap/filter IPs re-verified this session against the **real silicon RTL tree** (`sw/tt-oca-hw-main/hw/sep/sep_pkg.sv`, `sep_system_peripherals.sv`, `sep_system_csr.sv`, `meta/registers/svh/och_sep_top_reg.svh`) rather than only the bundled `rtl-details`/specification snapshot (`docs/01_sep_output_remap_ctrl_Specification/`). Every constant (`AP_REGION_BASE`, `STEE_REGION_BASE`, `NUM_REGIONS=16` for both, `IDX_START=19` for both) and the register stride (8 bytes, confirmed via the RTL's own `"0x8 spacing: bits [6:3] for 16 regions"` comment) matched exactly, with zero new discrepancies found.

### 7.2 `data_socket`'s Fixed Window Is Always Fully Indexable

Because the index-replace algorithm has no "no match" branch (§1.2), every address that reaches `data_socket` — as long as it falls within the 8 MB window (`16 regions × 512 KB`) the SEP bus routes here — is guaranteed to produce a valid index and a remapped address. There is no analogue of `local_master_alias_remap_ctrl`'s "passthrough on no match" or `sep_filter_ctrl`'s "deny on no match" to reason about here.

### 7.3 Reset Sensitivity

Same note as the other two peripherals documented this session: `reset_handler()` is sensitive to `rst_ni` on any edge, with the handler's own `if (!rst_ni.read())` guard making this observably identical to negedge-only sensitivity.

---

## 8. Conclusion

### 8.1 Summary

`sep_output_remap_ctrl` provides a bit-exact, dual-instantiated (AP/STEE) index-replace address remapper:

1. **Independently re-audited against real silicon RTL** this session — the most rigorously cross-checked of the four core SEP remap/filter IPs, with zero new bugs found.
2. **Structurally distinct from `local_master_alias_remap_ctrl`** despite the shared "remap controller" naming — replace vs. add, always-matches vs. range-checked, 512 KB vs. 4 KB granularity.
3. **Two permanent, well-documented TLM gaps** (AXI user retagging, CSR byte-enable), both shared library/TLM-2.0 limitations rather than defects in this peripheral's own logic.

### 8.2 Known Gaps

| Gap | Risk | Mitigation |
|-----|------|------------|
| AXI user-field retagging (`UserOverrideVal`) not modeled | Low — no downstream VP consumer inspects a user/source-ID sideband today | `Abstractions.md` §1.4; would need a custom TLM extension |
| CSR byte-enable not honored | Low — no firmware access pattern found uses partial-width accesses | `Abstractions.md` §1.5; shared `csml` library limitation |

---

**End of High-Level Design Specification**
