# Local Master Alias Remap Controller SystemC TLM High-Level Design Specification

**Document Version:** 1.0
**Date:** 2026-07-27
**IP Module:** SEP Local Master Alias Remap Controller (`local_master_alias_remap_ctrl`)
**Modeling Approach:** SystemC TLM2.0, 16-region programmable additive address remapper

---

## Executive Summary

This document specifies the SystemC Transaction-Level Model (TLM) for `local_master_alias_remap_ctrl`, the 16-entry programmable address remapper placed on the outbound AXI path of SEP's local master initiators (DMA, and similar on-chip engines that address the "system peripherals" region — see §7.3). It intercepts outgoing transactions and translates their address using a software-programmed `[start, end)` range + additive-offset table, forwarding the (possibly-translated) result to the SEP system crossbar.

### Key Architectural Characteristics

- **Additive, not replacement, remap** — each of the 16 regions adds a 44-bit offset to the address's upper bits on a range hit; this is the opposite algorithm from `sep_output_remap_ctrl` (AP/STEE), which *replaces* upper bits by table index rather than adding an offset. The two IPs share a naming pattern ("remap") but implement genuinely different hardware.
- **First-match (LZC) priority** — regions are evaluated `0..15` in order; the first `valid` region whose range contains the address wins. This mirrors the real RTL's leading-zero-count priority encoder exactly (a plain sequential loop with `break`-on-first-hit is functionally identical to an LZC over a hit-vector).
- **Passthrough on no match** — unlike the SEP output remap or filter peripherals (which deny/error on no match), an address matching no valid region is forwarded completely unchanged. There is no "block by default" concept here.
- **4 KB page granularity** — the lower 12 address bits are never touched by the addition; only bits `[55:12]` participate, with overflow silently discarded (44-bit truncation), exactly matching the real RTL's `prim_carry_select_adder`.
- **`cacheable` override applied on a hit** — the matched region's bit replaces the transaction's cache attribute, carried on `sep_axi_extension` since `tlm_generic_payload` has no `acache` field; passthrough on a miss.

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

Each of the 16 regions defines:
- A `[start, end)` address range the region is active for (44-bit fields, 4 KB aligned).
- A 44-bit **additive offset** applied to the upper address bits on a hit.
- A `valid` enable bit — regions are inactive at reset until firmware programs them.
- A `cacheable` bit that overrides the AXI `acache` sideband on a region hit (see §2.1.3).

This block is distinct from `sep_output_remap_ctrl` (AP/STEE), despite superficial naming similarity:

| | `output_remap` (AP/STEE) | `local_master_alias_remap_ctrl` |
|---|---|---|
| Match | Index from upper bits of a fixed window | Explicit `[start, end)` range check per region |
| Operation | **Replace** upper bits with table entry | **Add** offset to upper bits |
| No-match | Never (every address in the fixed window always indexes something) | Passthrough unchanged |
| Granularity | 512 KB (`IdxStart=19`) | 4 KB page (`IdxStart=12`) |

### 1.2 Purpose of This Document

This high-level design specification defines the SystemC TLM implementation for VP integration. It documents:

- Features included and excluded from the TLM model
- The data-path and CSR port interfaces
- The complete 16-region register map
- The remap algorithm, bit-exact against `axi_alias_remap.sv`
- Modeling assumptions, including what the `cacheable` override does and does not reach

### 1.3 Modeling Goals

1. **Bit-exact address translation** — for any firmware-programmed region table, the remapped address for a given input must match real RTL exactly, including overflow-discard behavior at the 44-bit boundary.
2. **Correct priority-encoder semantics** — first (lowest-index) valid matching region wins, even if multiple regions' ranges overlap.
3. **Zero-recompile region configuration** — firmware programs all 16 regions purely through the CSR interface (`REGION_START`/`REGION_END`/`REGION_ATTRS`); nothing about the region table is hardcoded in the model beyond the fixed count of 16 and 4 KB granularity.

### 1.4 Target Audience

- SystemC model developers maintaining or extending `local_master_alias_remap_ctrl`
- VP architects wiring this block into the SEP local-master data path (see `local_alias_remap_adapter` in `vp/platform/sep/inc/adapters.h`, and the broader system-peripherals routing discussion in `Abstractions.md` §3.1)
- SEP firmware engineers programming the 16-region table
- Verification engineers creating or extending the test plan

---

## 2. Features

### 2.1 Is (MUST Model)

#### 2.1.1 16-Region Programmable Table

Backed by three parallel `csml_reg_vector<..., 16>` arrays (`REGION_START`, `REGION_END`, `REGION_ATTRS`), each region occupying 3 of 4 words in a 0x20-byte stride (word 3 reserved). All fields reset to 0 (all regions invalid, all ranges/offsets zero).

#### 2.1.2 Remap Algorithm (`match_region()` + `apply_offset()`)

Implements `axi_alias_remap.sv`'s LZC + carry-select-adder behavior exactly as a sequential scan. Region lookup and address translation are separate calls because the `cacheable` override in §2.1.3 needs the winning region index, not just the translated address:

```
match_region(addr):
    for r in 0..15:
        skip if !REGION_ATTRS[r].valid
        skip if !(addr >= REGION_START[r].start_addr && addr < REGION_END[r].end_addr)
        return r            // first match wins
    return -1               // no match: passthrough unchanged

apply_offset(addr, r):
    addr_upper   = (addr   & upper_mask) >> 12
    offset_upper = (offset & upper_mask) >> 12
    sum_upper    = (addr_upper + offset_upper) & (upper_mask >> 12)   // 44-bit overflow discarded
    return (sum_upper << 12) | (addr & lower_mask)
```

`lower_mask`/`upper_mask` are derived once at construction from `IDX_START=12` (4 KB granularity): `lower_mask = 0xFFF` (bits `[11:0]`, always preserved verbatim from the input address), `upper_mask = ADDR_FIELD_MASK & ~lower_mask` (bits `[55:12]`, where the addition happens).

#### 2.1.3 `cacheable` Override

On a region hit the transaction's cache attribute is replaced by `REGION_ATTRS[r].cacheable`, matching `axi_alias_remap.sv:122,147`:

```systemverilog
assign axi_out_req_o.aw.cache = no_write_hit ? axi_in_req_i.aw.cache
                                             : {(axi_pkg::CacheWidth){aw_remapped_cacheable}};
```

It is an override rather than a merge — a region with `cacheable = 0` clears an incoming set bit just as a region with `cacheable = 1` sets a clear one. On a miss the incoming value passes through untouched, the same "compute on hit, passthrough on miss" shape as the address itself.

`tlm_generic_payload` has no cache field, so the bit travels on `sep::sep_axi_extension::cacheable` (`sep/utils/tlm_extensions/sep_axi_extension.h`). RTL drives all four `AxCACHE` bits from this one region bit, so a single `bool` is a faithful representation. Transactions arriving without the extension carry no cache attribute for the model to override, and are forwarded with the address remapped only.

#### 2.1.4 Data-Path Address and Attribute Restore

`data_b_transport()`/`data_transport_dbg()` save the original address and cache attribute, overwrite both for the forwarded transaction, then restore them on the payload afterward — so the caller's own copy of the transaction is never left mutated. In RTL these are separate downstream wires; the restore is what makes a shared TLM payload behave like the hardware's fan-out.

#### 2.1.5 Testbench Backdoor (`get_region()`)

Decodes any of the 16 regions into a plain `Region{start_addr, end_addr, offset, cacheable, valid}` struct for test assertions, without needing to reconstruct field extraction logic in every test.

#### 2.1.6 Reset

`SC_METHOD` sensitive to any edge of `rst_ni` (not just the negative edge — see §7.2), calling `reset_all_registers()` whenever `rst_ni` reads low, clearing all 16 regions to invalid/zero.

### 2.2 Is Not (EXCLUDE)

#### 2.2.1 Cache Attribute Semantics Beyond the Region Bit

The `cacheable` override itself is modelled (§2.1.3), but nothing downstream in the VP acts on the resulting attribute: no model changes its behaviour based on whether a transaction is cacheable, because there are no caches in the LT memory model to allocate into. The bit is carried faithfully so that a future AT model, or a checker looking for the RTL's attribute, sees the right value — it is not a functional input to any current target.

The four `AxCACHE` bits are also collapsed to one `bool`. That is lossless for this IP, whose RTL replicates a single region bit across all four, but it means the extension cannot express the finer-grained encodings (bufferable, allocate hints) that a different AXI master might drive.

#### 2.2.2 Overlapping-Region Priority Beyond First-Match

The model correctly implements "first valid match wins," but does not separately validate or warn about firmware programming overlapping region ranges — if regions 2 and 5 both claim an address, region 2 silently wins with no diagnostic. This matches real hardware (which has no such diagnostic either), so it's not a gap in fidelity, just worth calling out since it can produce confusing results during firmware bring-up.

#### 2.2.3 Timing

All remap and register operations complete synchronously within `b_transport()`; there is no explicit `sc_time` delay associated with the addition itself (real RTL's carry-select adder has a fixed, small combinational depth, not separately modeled here since this is an LT, not AT, model).

---

## 3. Port Interfaces

| Category | Port/Socket Name | Type | Direction | Description |
|----------|-------------------|------|-----------|--------------|
| **CSR Bus** | `target_socket` (via `local_alias_remap_base`) | `tlm_utils::simple_target_socket<csml_memory<64>, 32>` | Target | 64-bit TLM target for the 16-region CSR table. Memory window: `0x200` bytes (16 × 0x20 B stride). |
| **Data Path (in)** | `data_socket` | `tlm_utils::simple_target_socket<local_alias_remap_ip>` | Target | Incoming AXI transactions from SEP local masters (DMA, and other engines routed here — see §7.3 for the current VP wiring's scope). |
| **Data Path (out)** | `remapped_socket` | `tlm_utils::simple_initiator_socket<local_alias_remap_ip>` | Initiator | Forwards the (possibly remapped) transaction onward — in the current VP, re-injected onto the SEP `SimpleBus` as a new initiator (see `och_sep_ss.hpp`: `bus->tsocks[...].bind(local_alias_remap->remapped_socket)`). |
| **Reset** | `rst_ni` | `sc_in<bool>` | Input | Active-low; clears all 16 regions to invalid/zero when asserted. |

### 3.1 Interface Notes

1. **CSR base address is a VP integration detail, not encoded in the model** — `0x10A1_0000` (per `local_alias_remap.h`'s doc comment) is configured in `Args.hpp`/`och_sep_ss.hpp`, not hardcoded here.
2. **No CSR read/write restriction beyond the field masks** — every field is plain `sw=rw` per the RDL (`0xfffffffffff000` masks on `REGION_START`/`REGION_END`; `0xc0fffffffffff000` on `REGION_ATTRS` covering `offset`+`cacheable`+`valid`). There are no lock bits or WOSET semantics on this peripheral, unlike `sep_efuse` or `sep_filter_ctrl`.

---

## 4. Memory-Mapped Registers

16 regions, each occupying a 0x20-byte (4-word) stride; word 3 of each region is reserved/unused.

| Register | Word Offset (per region) | Bits | Field | Reset | Access | Description |
|----------|---------------------------|------|-------|-------|--------|-------------|
| **REGION_START** | 0 | [11:0] | Reserved0 | 0 | — | Always 0. |
| | | [55:12] | `start_addr` | 0x0 | RW | Region start address, 4 KB aligned, inclusive. |
| | | [63:56] | Reserved1 | 0 | — | Always 0. |
| **REGION_END** | 1 | [11:0] | Reserved0 | 0 | — | Always 0. |
| | | [55:12] | `end_addr` | 0x0 | RW | Region end address, 4 KB aligned, **exclusive**. |
| | | [63:56] | Reserved1 | 0 | — | Always 0. |
| **REGION_ATTRS** | 2 | [11:0] | Reserved0 | 0 | — | Always 0. |
| | | [55:12] | `offset` | 0x0 | RW | Additive address offset applied to bits `[55:12]` on a hit. |
| | | [62] | `cacheable` | 0 | RW | Overrides the transaction's cache attribute on a hit — §2.1.3. |
| | | [63] | `valid` | 0 | RW | Region active when 1. |
| — | 3 | — | Reserved | — | — | Unused stride padding. |

CSR span per region: `region_base + 0x00` (`REGION_START`), `+0x08` (`REGION_END`), `+0x10` (`REGION_ATTRS`), `+0x18` reserved. Total: `16 × 0x20 = 0x200` bytes.

---

## 5. Remap Algorithm

### 5.1 Bit-Level Walkthrough

Given `addr` (56-bit field-masked, per `ADDR_FIELD_MASK = 0x00FFFFFFFFFFF000`):

1. **Range check** (per region, in index order 0→15): `REGION_ATTRS[r].valid && addr >= REGION_START[r].start_addr && addr < REGION_END[r].end_addr`. First hit wins; no further regions are checked (equivalent to RTL's LZC over a per-region hit-vector).
2. **Additive combine**: only bits `[55:12]` of `addr` and `offset` participate in the addition; bits `[11:0]` of the result are always the original `addr`'s low 12 bits, untouched.
3. **Overflow discard**: the sum of `addr[55:12] + offset[55:12]` is masked back down to 44 bits (`upper_mask >> 12`) — any carry out of bit 43 is silently dropped, matching the real RTL's `prim_carry_select_adder` (which is a fixed-width adder with no overflow flag consulted by this logic).
4. **Cache attribute**: on a hit, the matched region's `cacheable` bit replaces the transaction's attribute, whichever way it was set (§2.1.3).
5. **No match**: if no region's range contains `addr`, both the address and the cache attribute are forwarded completely unchanged (passthrough) — there is no default-deny behavior on this peripheral, unlike the SEP inbound/outbound filters.

### 5.2 Worked Example

Region 3 configured: `start_addr=0x1_0000_0000`, `end_addr=0x1_0010_0000`, `offset=0x2_0000_0000`, `valid=1`. An incoming address `0x1_0005_0123`:
- Falls in `[0x1_0000_0000, 0x1_0010_0000)` → hit.
- `addr_upper = 0x1_0005_0` (bits `[55:12]`), `offset_upper = 0x2_0000_0`.
- `sum_upper = 0x3_0005_0` (no overflow in this example).
- Result: `0x3_0005_0123` — upper bits replaced by the sum, lower 12 bits (`0x123`) preserved verbatim.

---

## 6. Internal Architecture

### 6.1 Class Hierarchy

```
local_alias_remap_ip (sc_module, extends local_alias_remap_base)
├── local_alias_remap_base       (CSML register instances + target_socket)
│   ├── REGION_START[0..15]      @ region_base + 0x00 (stride 0x20)
│   ├── REGION_END[0..15]        @ region_base + 0x08
│   └── REGION_ATTRS[0..15]      @ region_base + 0x10
├── data_socket                  (AXI data-path slave)
├── remapped_socket               (AXI data-path master)
├── rst_ni                        (sc_in<bool>)
└── (private)
    ├── uint64_t lower_mask_      — bits [11:0], derived from IDX_START at construction
    └── uint64_t upper_mask_      — bits [55:12], derived from IDX_START at construction
```

### 6.2 Initialization Flow

```
local_alias_remap_ip constructor:
  → local_alias_remap_base constructor: allocate csml_memory<64>, construct
    REGION_START/END/ATTRS register-vectors, bind target_socket
  → SC_METHOD(reset_handler), sensitive to rst_ni (any edge)
  → derive lower_mask_ = (1 << 12) - 1
  → derive upper_mask_ = ADDR_FIELD_MASK & ~lower_mask_
  → register data_socket's b_transport/transport_dbg callbacks
```

### 6.3 Per-Transaction Flow

```
data_b_transport(trans, delay):
  orig_addr = trans.get_address()
  idx       = match_region(orig_addr)       // §5.1

  if idx < 0:                               // miss: address and cache attribute
      remapped_socket->b_transport(trans, delay)   //       both pass through
      return

  orig_cacheable = override_cacheable(trans, idx)  // §2.1.3
  trans.set_address(apply_offset(orig_addr, idx))
  remapped_socket->b_transport(trans, delay)
  trans.set_address(orig_addr)              // restore caller's view
  restore_cacheable(trans, orig_cacheable)
```

---

## 7. Modeling Assumptions

### 7.1 Register Access

1. **No lock/WOSET semantics** — unlike `sep_efuse`'s WOSET fields or `sep_filter_ctrl`'s `locked` bit, every field here is plain `sw=rw`. Firmware can freely reconfigure any region at any time, including while it is actively being matched against in-flight traffic — there is no "freeze configuration" mechanism to model or omit.

### 7.2 Reset Sensitivity

2. **`reset_handler()` is sensitive to `rst_ni` (both edges), not `rst_ni.neg()` only** — functionally equivalent here since the handler itself checks `if (!rst_ni.read())` before acting, but worth noting as a minor style difference from `el2_pic`'s negedge-only sensitivity list; both produce identical observable behavior since a positive edge simply re-enters the method and does nothing.

### 7.3 VP Integration Scope (Cross-Reference)

3. **What actually reaches this peripheral's `data_socket` today** — per `Abstractions.md` §3.1 and §2.2, the current VP wiring routes CPU/DMA traffic through the *fixed* local-alias remap (`local_alias_remap_adapter`) into this model only for addresses in the `[0xC0000000, 0xFFFFFFFF]` window; traffic bound for mailbox/system-CSR/other-remap-CSR address ranges bypasses this model entirely today, even though real hardware routes all "system peripherals" traffic through it. This is a known, deliberately-deferred VP limitation, not a defect in this peripheral's own algorithm — the remap logic documented in §5 is bit-exact against RTL regardless of which addresses currently reach it in the VP.

4. **This model is a single shared instance for all local masters** — real RTL's `u_local_master_remap_wrap` sits once in `sep_system_peripherals.sv`, downstream of the *fixed* per-master remap (`axi_local_alias_remap`, one instance per CPU channel + one for DMA); this VP mirrors that by having exactly one `local_alias_remap_ip` instance, fed by a single shared `local_alias_remap_adapter`. No functional difference results from combining the fixed-remap stage across masters (see `Abstractions.md` §2.2).

### 7.4 Address Width

5. **56-bit field masking (`ADDR_FIELD_MASK`) is applied uniformly** — the model does not distinguish "addresses that were already 56-bit" from "addresses truncated down from something wider"; any address handed to `match_region()`/`apply_offset()` is masked to `[55:0]` before the range check, matching how the real RTL's address bus width is fixed at 56 bits system-wide for this block.

---

## 8. Conclusion

### 8.1 Summary

The `local_master_alias_remap_ctrl` SystemC TLM model provides a bit-exact, 16-region programmable additive address remapper:

1. **First-match priority encoding** — a straightforward sequential scan, functionally identical to real RTL's LZC-based hit-vector priority.
2. **44-bit additive combine with overflow discard** — matches `prim_carry_select_adder`'s fixed-width truncation behavior exactly.
3. **Passthrough-on-no-match** — the only SEP remap/filter peripheral in this VP with no default-deny behavior.
4. **Zero-recompile firmware configuration** — all 16 regions are purely CSR-programmable, no VP-side hardcoding beyond region count and page granularity.

### 8.2 Known Gaps

| Gap | Risk | Mitigation |
|-----|------|------------|
| No VP target acts on the resulting cache attribute, and the four `AxCACHE` bits are collapsed to one `bool` | Low — the override matches RTL and is observable; there are no caches at LT for it to drive, and this IP's RTL replicates a single bit across all four | §2.2.1; revisit if an AT model or a finer-grained AXI master needs the full encoding |
| Overlapping-region firmware misconfiguration produces no diagnostic | Low — matches real hardware behavior exactly | N/A (not a fidelity gap) |
| Not all "system peripherals" traffic currently reaches this model in the VP | Low in practice (dormant — no existing firmware test exercises the affected address overlap) | Tracked in `Abstractions.md` §3.1; deliberately deferred |

---

**End of High-Level Design Specification**
