# SMC Fabric — Register Interface

All fabric configuration registers are accessed through the local crossbar's
internal CSR sub-crossbar.  Software uses the same AXI4 path as all other
local-space accesses; the fabric decodes the address range and dispatches to
the appropriate internal handler.

All offsets are relative to `LOCAL_BASE` (default `0xC000_0000`).

In the LT model, `b_transport` writes to the remap and filter sockets update
the fabric's internal tables immediately.  There is no shadow-register latency.

---

## 1. Global Fabric CSRs

Served directly by the fabric's `b_transport` handler for the CPU control
window at `LOCAL_BASE + 0x001_0000`.

Offsets/resets per `ocah_smc_top_reg.svh` (`SMC_CPU_CTRL_*`).

| Offset | Register | Access | Reset | Description |
|--------|----------|--------|-------|-------------|
| `0x10040` | `GLOBAL_BASE` | RW | `0x4000_0000` | Global-alias base. A write updates the routing parameter and triggers DMI invalidation on all initiator sockets. |
| `0x10048` | `LOCAL_BASE` | RO | `0xC000_0000` | Local aperture base. Returns the constructor-time `local_base_addr` value. |
| `0x10050` | `REGION_SIZE` | RW | `0x0200_0000` | Aperture window size (32 MiB default). Write triggers DMI invalidation. |

---

## 2. Alias Remap Registers (`ALIAS_REMAP`)

Base: `LOCAL_BASE + 0x001_2000`

8 entries, 0x20 bytes each.  Stride: `N × 0x20`.

| Offset within entry | Field | Access | Description |
|--------------------|-------|--------|-------------|
| `+0x00` | `REGION_START` | RW | Aligned start address of the region (4 KiB granularity) |
| `+0x08` | `REGION_END` | RW | Exclusive end address |
| `+0x10` | `REGION_OFFSET` | RW | Signed offset added to addresses within the region |
| `+0x18` | `REGION_ATTRS` | RW | `[0]` = `valid` — enable this entry; `[1]` = `cacheable` — propagated to AXI user field |

**LT side-effects on write:** Rebuilds the internal `alias_regions_[8]` table.
Calls `invalidate_direct_mem_ptr(0, UINT64_MAX)` on all initiator sockets.

**Constants:**

| Name | Value | Description |
|------|-------|-------------|
| Number of entries | 8 | |
| Address granularity | 4 KiB (bits [11:0] of all address fields are zero) | |

---

## 3. M-Mode Remap Registers (`MMODE_REMAP`)

Base: `LOCAL_BASE + 0x001_3000`

8 entries, 0x08 bytes each.  Stride: `N × 0x08`.  Bit-exact image of
`output_remap.rdl` / `output_remap.sv` (lines 52–61): each entry is a single
64-bit `REGION_ATTRS` register and there is **no valid bit** — every region is
always active.

| Offset within entry | Field | Access | Description |
|--------------------|-------|--------|-------------|
| `+0x00` | `REGION_ATTRS.offset[31:0]`  | RW | Low 32 bits of the 56-bit translation offset |
| `+0x04` | `REGION_ATTRS.offset[55:32]` | RW | High 24 bits (a 64-bit access at `+0x00` reads/writes the whole field) |

**Behaviour:** Bit-exact to `output_remap.sv`.  For a transaction whose address
falls within the M-mode window (`MMODE_REMAP_START … +MMODE_REMAP_SIZE`,
relative to `LOCAL_BASE` or `GLOBAL_BASE`):

```
adjusted = addr - window_base
idx      = adjusted[OUTPUT_REMAP_IDX_START +: clog2(8)]   // bits [22:20]
out      = { offset[55:OUTPUT_REMAP_IDX_START], adjusted[OUTPUT_REMAP_IDX_START-1:0] }
```

i.e. the 8 regions tile the 8 MiB window in 1 MiB (`IdxStart = 20`) slots; the
selected region's `offset[55:20]` replaces the upper address bits while the low
20 bits are preserved.  The `smc_axi_extension.source_id` is overridden to
`MMODE_SRC_ID`.  (At reset `offset == 0`, matching the RTL default.)

**LT side-effects on write:** Updates `mmode_regions_[idx].offset` (raw register
image).  Triggers DMI invalidation.

---

## 4. Xvisor Remap Registers (`XVISOR_REMAP`)

Base: `LOCAL_BASE + 0x001_4000`

8 entries, 0x08 bytes each.  Same layout as M-mode remap — a single 64-bit
`REGION_ATTRS.offset[55:0]` per entry, no valid bit.

| Offset within entry | Field | Access | Description |
|--------------------|-------|--------|-------------|
| `+0x00` | `REGION_ATTRS.offset[31:0]`  | RW | Low 32 bits of the 56-bit translation offset |
| `+0x04` | `REGION_ATTRS.offset[55:32]` | RW | High 24 bits |

**Behaviour:** Xvisor (H-mode) traffic in the Xvisor window is remapped using
the same `{offset[55:20], adjusted[19:0]}` encoding as M-mode (see §3) and
tagged with `OTHERS_SRC_ID`.

**LT side-effects on write:** Updates `xvisor_regions_[idx].offset`.  Triggers
DMI invalidation.

---

## 5. Inbound Filter Registers

Base: `LOCAL_BASE + 0x001_5000` (`SMC_INBOUND_FILTER_CTRL_0`, decoded by the
internal CSR crossbar).  16 filter entries, stride `0x20` (entry *N* at
`base + N×0x20`).  Each entry is a bit-exact image of `filter_ctrl.rdl` — three
64-bit registers:

| Offset within entry | Register | Access | Description |
|--------------------|----------|--------|-------------|
| `+0x00` | `FILTER_CONFIG` | RW | `read_en[0]`, `write_en[1]`, `addr_mode[4]` (entry enable), `allow_ns[8]`, `data_bus_width[14:12]` (RO = 3), `src_id[19:16]`, `group_id[23:20]`, `allow_burst[24]`, `locked[63]` |
| `+0x08` | `START_ADDR` | RW | `start_addr[55:0]` — inclusive range start |
| `+0x10` | `END_ADDR` | RW | `end_addr[55:0]` — inclusive range end (reset `0x7`) |
| `+0x18` | — | — | Reserved within the `0x20` stride |

`accesswidth = 64`: a 64-bit access at the register offset reads/writes the
whole field; 32-bit accesses see the low (`+0x0`) or high (`+0x4`) word.

**Matching (bit-exact to `axi_filter_wrap`/`traffic_filter`):** a filter *hits*
when `addr_mode = 1` **and** the (granularity-snapped) address lies in
`[start_addr, end_addr]` **and** the source-ID matches (`src_id = 0` ⇒ ignored,
else exact match) **and** the NS bit matches exactly (`tx_ns == allow_ns`).
Group-ID filtering is disabled in the SMC (`EnGroupIdFilter = 0`).  The
lowest-index hitting filter wins; the transaction is then permitted iff that
filter's `read_en`/`write_en` (for the access direction) is set.  Address
granularity is 4 KiB when `allow_burst = 1`, otherwise 8 B (`data_bus_width`).

**Default policy:** Deny all (`BlockByDefault = 1` in `smc_input_fabric`) — when
no filter hits, the transaction is blocked.

**`locked`:** write-one-to-set; once set, further writes to that entry's
registers are ignored.

**LT side-effects on write:** Updates `inbound_filter_[16]` immediately.  Bursts
are not modelled in LT (single-beat payloads), so `allow_burst` only selects the
match granularity.

---

## 6. Outbound Filter Registers

Base: `LOCAL_BASE + 0x001_6000` (`SMC_OUTBOUND_FILTER_CTRL_0`).  Same entry
layout and matching rules as the inbound filter.

**Default policy:** Allow all (`BlockByDefault = 0` in `smc_output_fabric`) —
when no filter hits, the transaction passes.

**LT side-effects on write:** Updates `outbound_filter_[16]` immediately.

---

## 7. Register Access Semantics

| Operation | LT behaviour |
|-----------|-------------|
| Any register read | Returns current model state; adds `reg_access_ns` to `delay` |
| Write `GLOBAL_BASE` | Updates `global_base_` routing parameter; triggers `invalidate_all_dmi()` |
| Write `REGION_SIZE` | Updates `region_size_` routing parameter; triggers `invalidate_all_dmi()` |
| Write `ALIAS_REMAP[N].*` | Rebuilds alias remap table; triggers `invalidate_all_dmi()` |
| Write `MMODE_REMAP[N].*` | Rebuilds M-mode remap table; triggers `invalidate_all_dmi()` |
| Write `XVISOR_REMAP[N].*` | Rebuilds Xvisor remap table; triggers `invalidate_all_dmi()` |
| Write inbound/outbound filter | Rebuilds the relevant filter table |
| `get_direct_mem_ptr` | Always returns `dmi_allowed = false` |

---

## 8. Source-ID Constants

Values per `smc_pkg.sv` (`OTHERS_SRC_ID`, `SMC_SRC_ID`, `MMODE_SRC_ID`, `SEP_SRC_ID`).

| Constant | Value | Applied on |
|----------|-------|-----------|
| `OTHERS_SRC_ID` | `0x0` | After Xvisor remap |
| `SMC_SRC_ID` | `0x3` | Plain outbound path (non-M-mode, non-Xvisor) |
| `MMODE_SRC_ID` | `0xC` | After M-mode remap |
| `SEP_SRC_ID` | `0xF` | SEP inbound traffic (not remapped in the LT model) |
