# local_master_alias_remap_ctrl — Model Plan

---

## 1. What Is This Unit?

`local_master_alias_remap_ctrl` is a **16-entry programmable address remapper** placed on the
outbound AXI paths of SEP local master initiators (DMA, SPAcc, PKA, and similar on-chip engines).
It intercepts outgoing AXI transactions and translates their addresses using a software-programmed
range-and-offset table before forwarding them to the system crossbar.

Each of the 16 entries defines:
- A `[start, end)` address range that the region is active for.
- A 56-bit **additive offset** applied to the upper address bits when there is a hit.
- A `valid` enable bit — regions are inactive at reset until firmware programs them.
- A `cacheable` hint that overrides the AXI `acache` sideband (irrelevant in LT VP).

The block is distinct from `ap_output_remap_ctrl` / `stee_output_remap_ctrl`:

| | `output_remap` (AP/STEE) | `alias_remap` (local master) |
|---|---|---|
| Match | Fixed index from upper bits of a fixed window | Explicit `[start, end)` range check |
| Operation | **Replace** upper bits with table entry | **Add** offset to upper bits |
| No-match | Never (all addresses in window always index) | Passthrough unchanged |
| Granularity | 512 KB (IdxStart=19) | 4 KB page (IdxStart=12) |

---

## 2. Where It Sits in the SEP

```
SEP local master (DMA / SPAcc / PKA)
           │
           │  AXI out
           ▼
   AxiAliasRemap.data_socket
           │
    range/offset table lookup (16 regions)
           │  hit: addr += offset (upper bits)
           │  miss: passthrough unchanged
           ▼
   AxiAliasRemap.remapped_socket ──► SEP system crossbar / SimpleBus
```

**CSR registers (SW configures the table through here):**

```
SEP RISC-V CPU
    │  AXI-Lite
    ▼
local_master_alias_remap_ctrl[0..15]
Base 0x10A1_0000, 16 × 0x20 B stride = 0x200 B total
```

**CSR address map:**

| Instance | Base | REGION_START | REGION_END | REGION_ATTRS |
|---|---|---|---|---|
| `[0]` | `0x10A1_0000` | `+0x00` | `+0x08` | `+0x10` |
| `[1]` | `0x10A1_0020` | `+0x00` | `+0x08` | `+0x10` |
| … | … | | | |
| `[15]` | `0x10A1_01E0` | `+0x00` | `+0x08` | `+0x10` |

Total CSR span: `0x10A1_0000 – 0x10A1_01FF` (0x200 bytes).
Each instance uses 0x18 bytes (3 × 8 B registers); bytes `[0x18–0x1F]` per instance are reserved.

---

## 3. Behaviour

### 3.1 Register Layout per Instance (from `alias_remap.rdl`)

| Offset | Register | Field | Bits | Access | Reset | Description |
|---|---|---|---|---|---|---|
| `0x00` | `REGION_START` | `start_addr` | [55:12] | RW | `0` | Region start address, 4 KB aligned |
| `0x08` | `REGION_END` | `end_addr` | [55:12] | RW | `0` | Region end address, exclusive, 4 KB aligned |
| `0x10` | `REGION_ATTRS` | `offset` | [55:12] | RW | `0` | Additive address offset |
| | | `cacheable` | [62] | RW | `0` | Override AXI acache bits if set |
| | | `valid` | [63] | RW | `0` | Region active when 1 |

All registers are 64-bit (`regwidth=64; accesswidth=64`). Bits `[11:0]` in all three registers
are always 0 (lower 12 bits tied to 0 in hardware and ignored on write).

### 3.2 Remap Algorithm (from `axi_alias_remap.sv`)

```
for r = 0 .. 15:
    if valid[r] && addr >= start_addr[r] && addr < end_addr[r]:
        upper   = addr[55:12] + offset[r][55:12]   // carry-select adder, overflow discarded
        remapped = { upper[43:0], addr[11:0] }      // lower 12 bits (page offset) preserved
        break   // first match wins  (LZC priority encoder in RTL)

if no match: remapped = addr  (passthrough)
```

Key points:
- **Additive**, not replacement — firmware sets `offset` to the delta, not the destination.
- **First match priority** — lower region index wins if ranges overlap.
- **4 KB granularity** — the 12 lower address bits are always preserved unchanged.
- **Passthrough on no match** — transactions outside all valid regions are forwarded as-is.

---

## 4. VP Model Code Assessment

### Model: `AxiAliasRemap` (`axi_alias_remap.h` / `axi_alias_remap.cpp`)

The model is a generic `SC_MODULE` parameterized by `(csr_base, num_regions, idx_start)`.
For the SEP: `csr_base = 0x10A10000`, `num_regions = 16`, `idx_start = 12`.

### What is correct

| Aspect | Verdict |
|---|---|
| Remap algorithm | Correct — for-loop is exact C++ equivalent of LZC + carry-select adder in RTL |
| Additive arithmetic with overflow discard | Correct — `(addr_upper + offset_upper) & (upper_mask_ >> idx_start_)` |
| Passthrough on no match | Correct — returns `addr` unchanged when loop exits without hit |
| First-match priority | Correct — loop iterates from index 0, breaks on first valid hit |
| REGION_START/END field mask `[55:12]` | Correct — `& 0x00FFFFFFFFFFF000` enforces 4 KB alignment |
| REGION_ATTRS: offset`[55:12]`, cacheable`[62]`, valid`[63]` | Correct per RDL |
| Reset: all regions invalid, all fields 0 | Correct |
| CSR stride 0x20 vs content 0x18 | Correct — stride matches address map; bytes 0x18–0x1F per instance return 0 / ignore write |
| Byte-enable masking on CSR writes | Correct |
| Address restore after forward | Correct — `trans.set_address(orig_addr)` after forward |
| `transport_dbg` on both sockets | Present |

### Cacheable bit — intentionally not propagated

`cacheable` is stored in the model but not applied to TLM payload. This is correct for LT — TLM-2.0
generic payload has no AXI `acache` field. If AT (Approximately Timed) modeling is added later,
this would need to be threaded through a TLM extension.

### No bugs found — model is ready to integrate

---

## 5. VP Integration

### Platform wiring (to be done in `och_sep_ss.hpp`)

```cpp
AxiAliasRemap local_alias_remap("local_alias_remap",
                                 0x10A10000UL,  // CSR base
                                 16,            // num_regions
                                 12);           // idx_start (4 KB granularity)

// CSR slave: bound to SimpleBus at 0x10A10000, span 0x200
bus.add_target(local_alias_remap.csr_socket, 0x10A10000, 0x10A10200);

// Data socket: bound to the address range that local masters (DMA, SPAcc, PKA) output
// In a passthrough-first VP, this can cover a broad window; unmatched addresses forward unchanged
bus.add_target(local_alias_remap.data_socket, <local_master_output_range>);

// Remapped socket: forwarded into the main SimpleBus or system crossbar
local_alias_remap.remapped_socket.bind(system_bus_socket);
```

> **Note on data-path binding:** Unlike `output_remap` (which has a fixed input window), this
> block's `data_socket` intercepts whatever address range the local masters produce. For initial
> VP bringup, the data_socket can be left unbound (local masters access the bus directly) and
> wired in once a firmware test exercises address remapping from a local master.

### What is not modelled
- `cacheable` bit effect on AXI acache signals — not applicable in TLM-2.0 LT.
- `remap_debug_o` port (DEBUG_OUTPUT=1 path in RTL) — not needed in VP.
- Multiple outstanding in-flight transactions — LT processes one at a time; sufficient for all
  current firmware tests.

### Firmware tests

`local_alias_sanity` in `fw/sep/tests/` exercises `SEP_LOCAL_BASE_ADDR` in `sep_cpu_ctrl` —
that is a CPU-side window remap, not this block. No current test directly exercises
`local_master_alias_remap_ctrl`; this block is listed as "not modelled" in the current VP.
The model is ready for when such tests are written.
