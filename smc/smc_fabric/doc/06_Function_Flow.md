# SMC Fabric — Function Flow Reference

**Document**: `06_Function_Flow.md`  
**Module**: `smc::smc_fabric` (`include/smc_fabric.h`, `src/smc_fabric.cpp`)  
**Status**: Reference — call graph and transaction-flow for the SystemC LT model  
**Companion docs**:
  - `01_overview_and_architecture.md` — purpose, block diagram, data-path flows
  - `02_tlm_interface.md` — sockets, signals, constructor parameters
  - `03_internal_architecture.md` — sub-block decomposition
  - `04_register_interface.md` — register map and CSR access semantics
  - `05_systemc_implementation.md` — C++ implementation patterns

---

## Contents

1. [Overview](#1-overview)
2. [Big picture — entry points](#2-big-picture--entry-points)
3. [Inbound master paths](#3-inbound-master-paths)
4. [Local routing (`route_local`)](#4-local-routing-route_local)
5. [Outbound routing (`route_outbound`)](#5-outbound-routing-route_outbound)
6. [Address remap](#6-address-remap)
7. [Filter logic](#7-filter-logic)
8. [Local address decode](#8-local-address-decode)
9. [Internal CSR handlers](#9-internal-csr-handlers)
10. [Reset and DMI](#10-reset-and-dmi)
11. [Error / deny path](#11-error--deny-path)
12. [Function inventory](#12-function-inventory)
13. [Key rules](#13-key-rules)

---

## 1. Overview

The SMC fabric is a **Bucket D pure TLM target**: it never calls `wait()` inside
`b_transport`, does not own a quantum keeper, and only **adds** `cfg_.reg_access_ns`
(default 1 ns) to the incoming `delay` on CSR accesses. It receives transactions
on six inbound target sockets and forwards them to downstream initiator sockets
(or handles remap/filter CSRs internally).

There is no `sc_out` single-driver hub like the UART — the fabric is a **pass-through
router**. Each transaction follows one of three inbound paths, then either
`route_local()` or `route_outbound()`.

| Property | Value |
|----------|-------|
| Temporal model | Loosely-timed (LT), pure target |
| Quantum keeper | None (initiator's responsibility) |
| DMI | Never granted (`get_direct_mem_ptr` always returns false) |
| Local/outbound split | Fixed **16 MB** window (`LOCAL_ALIAS_REGION_SIZE`) per aperture |
| Inbound filter default | **Block** (default-deny) |
| Outbound filter default | **Allow** (default-permit) |

---

## 2. Big picture — entry points

```mermaid
flowchart TB
    subgraph Inbound["Inbound target sockets"]
        JTAG["jtag_axi_in"]
        MMIO["mmio_in"]
        DACCEL["data_accel_in"]
        LOG["log_in"]
        SYS["sys_axi_in"]
        SEP["sep_axi_in"]
    end

    subgraph BT["b_transport callbacks"]
        BTI["bt_internal()"]
        BTJ["bt_jtag / bt_mmio /<br/>bt_data_accel / bt_log"]
        BTS["bt_sys_axi()"]
        BTSEP["bt_sep_axi()"]
    end

    subgraph PreRoute["Pre-routing"]
        AR["apply_alias_remap()"]
        IL["is_local()"]
        TLA["to_local_addr()"]
        IFL["inbound_filter_allow()"]
    end

    subgraph Route["Routing"]
        RL["route_local()"]
        RO["route_outbound()"]
    end

    subgraph OutLocal["Local initiators"]
        FP["to_front_port"]
        PER["to_periph"]
        DAC["to_data_accel_ctrl"]
        DFD["to_dfd_apb"]
        CPU["to_cpu_ctrl"]
        MB["to_mailbox"]
        DFT["to_dft_csr"]
        CSR["internal CSR handlers"]
    end

    subgraph OutGlobal["System output"]
        OUT["output_axi"]
    end

    JTAG --> BTJ --> BTI
    MMIO --> BTJ
    DACCEL --> BTJ
    LOG --> BTJ

    BTI --> AR --> IL
    IL -->|local| TLA --> RL
    IL -->|outbound| RO

    SYS --> BTS --> IFL
    IFL -->|allow| TLA --> RL
    IFL -->|deny| DENY["fill_deny_response()"]

    SEP --> BTSEP --> TLA --> RL

    RL --> FP & PER & DAC & DFD & CPU & MB & DFT & CSR
    RO --> OUT
```

---

## 3. Inbound master paths

Three distinct paths enter the fabric. All six sockets register
`get_direct_mem_ptr` → always denied.

### 3.1 Internal masters (JTAG, MMIO, DMA, Log)

```mermaid
flowchart TD
    T["bt_jtag / bt_mmio /<br/>bt_data_accel / bt_log"]
    T --> I["bt_internal()"]
    I --> AR["apply_alias_remap(addr)"]
    AR --> SET["trans.set_address(remapped)"]
    SET --> L{"is_local(addr)?"}

    L -->|yes| TLA["to_local_addr(addr)"]
    TLA --> RL["route_local()"]

    L -->|no| RO["route_outbound()"]
```

**Steps:**

1. `apply_alias_remap()` — scan `alias_regions_[0..7]`; on hit:
   `addr_out = addr_in + offset`; update `remap_debug_`.
2. `is_local()` — true if address falls in **either**:
   - `[local_base, local_base + 16 MB)`, or
   - `[global_base, global_base + 16 MB)`.
3. Local path: mask to 32-bit crossbar view via `to_local_addr()`.
4. Outbound path: forward to `route_outbound()` with full 64-bit address.

### 3.2 System AXI (`sys_axi_in`)

```mermaid
flowchart TD
    S["bt_sys_axi()"]
    S --> EXT["extract smc_axi_extension:<br/>source_id, prot[1]=NS"]
    EXT --> IF{"inbound_filter_allow()?"}
    IF -->|no| DENY["fill_deny_response()"]
    IF -->|yes| TLA["to_local_addr()"]
    TLA --> RL["route_local()"]
```

- **Always local** — no alias remap, no outbound path.
- Inbound filter: **default-deny** (`block_by_default = true`).
- Filter checks: address range, source ID, NS bit, read/write enable.

### 3.3 SEP AXI (`sep_axi_in`)

```mermaid
flowchart TD
    P["bt_sep_axi()"]
    P --> TLA["to_local_addr()"]
    TLA --> RL["route_local()"]
```

- **Bypasses** alias remap and inbound filter entirely.
- Can reach any decoded local target (including PLIC/CLINT above 16 MB).

---

## 4. Local routing (`route_local`)

Central decode function: maps a **32-bit masked local address** to either an
internal CSR handler or a downstream initiator socket.

```mermaid
flowchart TD
    RL["route_local(trans, delay)"]
    RL --> A["a = uint32_t(trans.get_address())"]

    A --> DFT{"DFT_CSR_BASE..END?"}
    DFT -->|yes| F1["to_dft_csr->b_transport()"]

    A --> CPU{"CPU_CTRL_BASE..END?"}
    CPU -->|yes| GCSR{"handle_global_csr()?"}
    GCSR -->|true| INT1["handled internally"]
    GCSR -->|false| F2["to_cpu_ctrl->b_transport()"]

    A --> AR{"AR_CTRL_BASE..END?"}
    AR -->|yes| H1["handle_alias_remap()"]

    A --> MR{"MR_CTRL_BASE..END?"}
    MR -->|yes| H2["handle_mmode_remap()"]

    A --> XR{"XR_CTRL_BASE..END?"}
    XR -->|yes| H3["handle_xvisor_remap()"]

    A --> IB{"IB_FILTER_BASE..END?"}
    IB -->|yes| H4["handle_inbound_filter()"]

    A --> OB{"OB_FILTER_BASE..END?"}
    OB -->|yes| H5["handle_outbound_filter()"]

    A --> MB{"MAILBOX_BASE..END?"}
    MB -->|yes| F3["to_mailbox->b_transport()"]

    A --> LD["local_decode(a)"]
    LD -->|socket| F4["sock->b_transport()"]
    LD -->|nullptr| DENY["fill_deny_response()"]

    F1 & F2 & F3 & F4 & INT1 & H1 & H2 & H3 & H4 & H5 --> DELAY["delay += reg_access_ns"]
```

### Local address map (absolute, base `0xC000_0000`)

| Range | Handler / socket |
|-------|------------------|
| `0xC000_0000` – `0xC000_1000` | `to_front_port` (WDT/debug) |
| `0xC000_2000` – `0xC000_B800` | `to_periph` (main) |
| `0xC000_C000` – `0xC000_D000` | `to_periph` (VP-only AOU park) |
| `0xC003_8000` – `0xC004_0000` | `to_data_accel_ctrl` |
| `0xC004_0000` – `0xC016_0000` | `to_front_port` (SPM) |
| `0xC016_0000` – `0xC026_0000` | `to_dfd_apb` |
| `0xC040_0000` – `0xC080_0000` | `to_periph` (ext) |
| `0xC400_0000` – `0xC800_0000` | `to_front_port` (PLIC) |
| `0xC800_0000` – `0xC802_0000` | `to_front_port` (CLINT/BEU) |
| `0xC000_B800` – `0xC000_C000` | `to_dft_csr` |
| `0xC001_0000` – `0xC001_2000` | `handle_global_csr` / `to_cpu_ctrl` |
| `0xC001_2000` – `0xC001_3000` | `handle_alias_remap` (internal) |
| `0xC001_3000` – `0xC001_4000` | `handle_mmode_remap` (internal) |
| `0xC001_4000` – `0xC001_5000` | `handle_xvisor_remap` (internal) |
| `0xC001_5000` – `0xC001_6000` | `handle_inbound_filter` (internal) |
| `0xC001_6000` – `0xC001_7000` | `handle_outbound_filter` (internal) |
| `0xC001_8000` – `0xC003_8000` | `to_mailbox` |
| (no match) | `fill_deny_response()` |

---

## 5. Outbound routing (`route_outbound`)

Used when internal masters (`bt_internal`) target an address outside the
16 MB local alias window.

```mermaid
flowchart TD
    RO["route_outbound(trans, delay)"]
    RO --> EXT["extract smc_axi_extension"]

    EXT --> NAR{"cfg_.no_addr_remap?"}
    NAR -->|false| CLS["classify_outbound(addr)"]

    CLS --> M["mmode"]
    CLS --> X["xvisor"]
    CLS --> P["plain"]

    M --> AOR1["apply_output_remap(mmode_regions_,<br/>MMODE_REMAP_START, MMODE_SRC_ID)"]
    X --> AOR2["apply_output_remap(xvisor_regions_,<br/>XVISOR_REMAP_START, OTHERS_SRC_ID)"]
    P --> SID["ext->source_id = SMC_SRC_ID"]

    AOR1 & AOR2 & SID --> SET["trans.set_address(remapped)"]
    NAR -->|true bypass| OFL
    SET --> OFL{"outbound_filter_allow()?"}

    OFL -->|no| DENY["fill_deny_response()"]
    OFL -->|yes| OUT["output_axi->b_transport()"]
```

### Outbound path classification (`classify_outbound`)

| Path | Address window (relative to local/global base) | Source ID after remap |
|------|-----------------------------------------------|----------------------|
| **M-mode** | `[0x0100_0000, 0x0180_0000)` (8 MB) | `0xC` (MMODE_SRC_ID) |
| **Xvisor** | `[0x0180_0000, 0x0200_0000)` (8 MB) | `0x0` (OTHERS_SRC_ID) |
| **Plain** | Everything else | `0x3` (SMC_SRC_ID) |

Outbound filter: **default-allow** (`block_by_default = false`).

---

## 6. Address remap

### 6.1 Alias remap (inbound, internal masters only)

```mermaid
flowchart LR
    IN["addr_in"] --> SCAN["scan alias_regions_[0..7]"]
    SCAN --> HIT{"valid &&<br/>start <= addr < end?"}
    HIT -->|yes| OUT["addr_out = addr + offset<br/>remap_debug_.hit = true"]
    HIT -->|no| PASS["addr_out = addr<br/>remap_debug_.hit = false"]
```

- Programmed via `handle_alias_remap()` → `handle_remap_entry()`.
- Each entry: `start`, `end`, signed `offset`, `valid`, `cacheable`.
- Skipped when `cfg_.no_addr_remap == true` is only for **output** remap;
  alias remap is always applied for internal masters.

### 6.2 Output remap (M-mode / Xvisor outbound)

```mermaid
flowchart TD
    A["addr"] --> BASE["compute window base<br/>(local or global + window_start)"]
    BASE --> ADJ["adjusted = addr - base"]
    ADJ --> IDX["idx = adjusted[22:20]"]
    IDX --> OFF["offset = table[idx].offset[55:0]"]
    OFF --> OUT["out = {offset[55:20], adjusted[19:0]}"]
    OUT --> SID["ext->source_id = override"]
```

Bit-exact to `output_remap.sv`: 8 regions × 1 MB (`IdxStart=20`).

### 6.3 Local address masking (`to_local_addr`)

```
local_addr[31:25] = cfg_.local_base_addr[31:25]   (forced)
local_addr[24:0]  = addr[24:0]                    (from transaction)
```

Makes local-alias and global-alias transactions decode identically in the
local crossbar.

---

## 7. Filter logic

```mermaid
flowchart TD
    FL["filter_lookup(table, addr, src_id, ns, is_write, block_by_default)"]
    FL --> LOOP["for each entry (lowest index wins)"]
    LOOP --> E1{"addr_mode?"}
    E1 -->|no| NEXT["next entry"]
    E1 -->|yes| RNG{"addr in [start, end]<br/>(4 KiB or 8 B granularity)?"}
    RNG -->|no| NEXT
    RNG -->|yes| SRC{"src_id match<br/>(0 = don't care)?"}
    SRC -->|no| NEXT
    SRC -->|yes| NS{"ns == allow_ns?"}
    NS -->|no| NEXT
    NS -->|yes| RET["return read_en or write_en"]
    NEXT --> LOOP
    LOOP -->|no hit| DEF["return !block_by_default"]
```

| Wrapper | Table | Default on no hit |
|---------|-------|-------------------|
| `inbound_filter_allow()` | `inbound_filter_[0..15]` | **Deny** (`block_by_default=true`) |
| `outbound_filter_allow()` | `outbound_filter_[0..15]` | **Allow** (`block_by_default=false`) |

Filter CSRs programmed via `handle_inbound_filter()` / `handle_outbound_filter()`
→ `handle_filter_entry_csr()` (shared static helper in `.cpp`).

---

## 8. Local address decode

```mermaid
flowchart TD
    LD["local_decode(a)"]
    LD --> FP{"FRONT ranges?<br/>WDT, SPM, PLIC, CLINT"}
    FP -->|yes| R1["return &to_front_port"]
    LD --> DAC{"DACCEL range?"}
    DAC -->|yes| R2["return &to_data_accel_ctrl"]
    LD --> DFD{"DFD range?"}
    DFD -->|yes| R3["return &to_dfd_apb"]
    LD --> PER{"PERIPH main/ext?"}
    PER -->|yes| R4["return &to_periph"]
    LD --> NULL["return nullptr"]
```

Note: CSR windows (CPU ctrl, remap, filter, mailbox, DFT) are handled **before**
`local_decode()` inside `route_local()`, not by this function.

---

## 9. Internal CSR handlers

```mermaid
flowchart TD
    subgraph Global["handle_global_csr()"]
        G1["GCSR_LOCAL_BASE — RO"]
        G2["GCSR_GLOBAL_BASE — RW"]
        G3["GCSR_REGION_SIZE — RW"]
        G2 & G3 --> INV["invalidate_all_dmi()"]
        GNONE["return false → forward to_cpu_ctrl"]
    end

    subgraph Alias["handle_alias_remap()"]
        A1["entry = sub_offset / 0x20"]
        A2["handle_remap_entry(alias_regions_[entry])"]
        A2 --> INV
    end

    subgraph Output["handle_mmode_remap() / handle_xvisor_remap()"]
        O1["entry = sub_offset / 0x08"]
        O2["handle_output_remap_entry(mmode/xvisor_regions_[entry])"]
        O2 --> INV
    end

    subgraph Filter["handle_inbound/outbound_filter()"]
        F1["entry = sub_offset / 0x20"]
        F2["handle_filter_entry_csr(filter_[entry])"]
    end
```

### CSR handler summary

| Handler | Table updated | Entry layout | Invalidate DMI |
|---------|---------------|--------------|----------------|
| `handle_global_csr()` | `cfg_.global_base_addr`, `cfg_.region_size` | 32-bit CSRs in CPU ctrl window | Yes (on write) |
| `handle_alias_remap()` | `alias_regions_[0..7]` | 4 × 64-bit fields / entry (0x20 stride) | Yes |
| `handle_mmode_remap()` | `mmode_regions_[0..7]` | 1 × REGION_ATTRS / entry (0x08 stride) | Yes |
| `handle_xvisor_remap()` | `xvisor_regions_[0..7]` | Same as M-mode | Yes |
| `handle_inbound_filter()` | `inbound_filter_[0..15]` | FILTER_CONFIG + START + END | No |
| `handle_outbound_filter()` | `outbound_filter_[0..15]` | Same layout | No |

Remap/filter CSR sockets (`to_aR_ctrl`, `to_mR_ctrl`, etc.) exist for testbench
observation but traffic is **handled internally** — tables update in-place without
forwarding to those sockets.

---

## 10. Reset and DMI

### Reset (`reset_proc`)

```mermaid
flowchart LR
    RST["rst_n_i negedge"] --> RP["reset_proc()"]
    RP --> CLR["clear alias/mmode/xvisor tables<br/>clear inbound/outbound filters<br/>clear remap_debug_"]
    CLR --> DEF["restore cfg_.global_base_addr = 0x4000_0000<br/>restore cfg_.region_size = 0x0200_0000"]
```

Sensitive to `rst_n_i.neg()` (active-low falling edge).

### DMI

```mermaid
flowchart LR
    GP["get_direct_mem_ptr()"] --> DENY["always return false"]
    INV["invalidate_all_dmi()"] --> NOOP["intentional no-op<br/>(hook for future use)"]
```

Every CSR write path that could affect routing calls `invalidate_all_dmi()`.
All `b_transport` exits set `trans.set_dmi_allowed(false)` on deny/internal-CSR paths.

---

## 11. Error / deny path

```mermaid
flowchart TD
    DENY["fill_deny_response(trans)"]
    DENY --> STAT["TLM_ADDRESS_ERROR_RESPONSE"]
    DENY --> DMI["set_dmi_allowed(false)"]
    DENY --> READ{"is_read?"}
    READ -->|yes| POISON["fill data with 0xBADCAB1E"]
```

**Callers of `fill_deny_response()`:**

| Caller | Condition |
|--------|-----------|
| `bt_sys_axi()` | Inbound filter denied |
| `route_local()` | `local_decode()` returned nullptr |
| `route_outbound()` | Outbound filter denied |
| CSR handlers | Entry index out of range |

---

## 12. Function inventory

### Construction

| Function | Role |
|----------|------|
| `smc_fabric(name)` | Default-construct with `config{}` |
| `smc_fabric(name, cfg)` | Register 6× `b_transport`, 6× `get_direct_mem_ptr`, `reset_proc` |

### TLM entry points

| Function | Socket | Path |
|----------|--------|------|
| `bt_jtag()` | `jtag_axi_in` | → `bt_internal()` |
| `bt_mmio()` | `mmio_in` | → `bt_internal()` |
| `bt_data_accel()` | `data_accel_in` | → `bt_internal()` |
| `bt_log()` | `log_in` | → `bt_internal()` |
| `bt_sys_axi()` | `sys_axi_in` | filter → `route_local()` |
| `bt_sep_axi()` | `sep_axi_in` | bypass → `route_local()` |
| `get_direct_mem_ptr()` | all targets | Always deny DMI |

### Routing core

| Function | Role |
|----------|------|
| `bt_internal()` | Alias remap → local/outbound split |
| `route_local()` | 32-bit decode → CSR handler or initiator forward |
| `route_outbound()` | Output remap → outbound filter → `output_axi` |
| `local_decode()` | Map address to `to_front_port` / `to_periph` / etc. |

### Address helpers

| Function | Role |
|----------|------|
| `apply_alias_remap()` | Inbound alias table lookup |
| `is_local()` | 16 MB aperture test (local + global base) |
| `to_local_addr()` | Mask to 32-bit crossbar view |
| `classify_outbound()` | M-mode / Xvisor / plain path |
| `apply_output_remap()` | Bit-exact `output_remap.sv` translation |

### Filter

| Function | Role |
|----------|------|
| `filter_lookup()` | Shared lowest-index-hit filter engine |
| `inbound_filter_allow()` | Default-deny wrapper |
| `outbound_filter_allow()` | Default-allow wrapper |

### CSR handlers

| Function | Role |
|----------|------|
| `handle_global_csr()` | LOCAL_BASE / GLOBAL_BASE / REGION_SIZE |
| `handle_remap_entry()` | Alias region field R/W |
| `handle_alias_remap()` | Dispatch to alias table entry |
| `handle_output_remap_entry()` | M-mode/Xvisor REGION_ATTRS R/W |
| `handle_mmode_remap()` | Dispatch to mmode table |
| `handle_xvisor_remap()` | Dispatch to xvisor table |
| `handle_inbound_filter()` | Dispatch to inbound filter entry |
| `handle_outbound_filter()` | Dispatch to outbound filter entry |

### Support

| Function | Role |
|----------|------|
| `fill_deny_response()` | Address error + poison read data |
| `invalidate_all_dmi()` | No-op hook (future DMI revocation) |
| `reset_proc()` | Clear tables; restore power-on CSR defaults |

### Testbench / debug API (public)

| Function | Role |
|----------|------|
| `read_global_base()` / `write_global_base()` | Inspect/mutate global aperture base |
| `read_region_size()` / `write_region_size()` | Inspect/mutate REGION_SIZE CSR image |
| `get_alias_region(n)` | Peek alias remap entry |
| `get_remap_debug()` | Last alias-remap hit info |
| `get_inbound_filter_entry(n)` | Peek inbound filter entry |
| `get_outbound_filter_entry(n)` | Peek outbound filter entry |

### File-local helpers (anonymous namespace in `.cpp`)

| Function | Role |
|----------|------|
| `filter_cfg_pack()` | Encode `filter_entry` → FILTER_CONFIG register |
| `filter_cfg_unpack()` | Decode FILTER_CONFIG write |
| `handle_filter_entry_csr()` | Shared filter CSR field R/W |

---

## 13. Key rules

### Pure target — no blocking waits

```
Initiator calls reg_socket->b_transport(trans, delay)
    |
    v
smc_fabric::bt_*()
    |
    +--> route_local() / route_outbound()
    |         |
    |         v
    |    downstream_sock->b_transport(trans, delay)   // nested LT call
    |
    +--> delay += reg_access_ns   (CSR paths only)
    |
    v
return to initiator (initiator owns quantum keeper sync)
```

The fabric **never** calls `wait()`. Routing latency is zero; only CSR touches
add `reg_access_ns`.

### Three inbound paths at a glance

```
Internal (jtag/mmio/dma/log):
  apply_alias_remap → is_local? → route_local | route_outbound

System (sys_axi_in):
  inbound_filter → to_local_addr → route_local

SEP (sep_axi_in):
  to_local_addr → route_local   (no remap, no filter)
```

### Internal vs forwarded CSRs

```
route_local()
  ├── handle_*() internally  → updates tables in smc_fabric state
  └── to_*->b_transport()    → forwards to bound downstream module
```

---

## Revision history

| Revision | Date | Notes |
|----------|------|-------|
| 0.1 | 2026-06-25 | Initial function-flow reference for `smc::smc_fabric` |
