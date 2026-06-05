# SEPMemory — High-Level Design

**IP Module:** SEPMemory  
**Abstraction:** SystemC TLM2.0  
**Location:** `sep/peripherals/sep_memory/`

---

## 1. Overview

`SEPMemory` is a single SystemC TLM module that serves all memory regions in the SEP platform — SRAM, ROM, ITCM, and DTCM — through a common `read_only` flag.  The module is backed by `PagedMemory` (`sep/utils/paged_mem.h`), a header-only sparse storage engine that lazily allocates 4 KB pages.

---

## 2. Component Layers

```
┌───────────────────────────────────────┐
│             SEPMemory                 │  sc_module + load_if
│  tsock  b_transport / get_dmi_ptr /   │  (sep/peripherals/sep_memory/)
│         transport_dbg                 │
└──────────────────┬────────────────────┘
                   │ uses
┌──────────────────┴────────────────────┐
│             PagedMemory               │  header-only pure C++
│  unordered_map<page_base, 4KB vector> │  (sep/utils/paged-memory/paged_mem.h)
└───────────────────────────────────────┘
```

### 2.1 SEPMemory (`sep_memory.h / sep_memory.cpp`)

Implements the TLM2.0 target socket interface and the `load_if` data-loading interface.  Delegates all storage to a `PagedMemory m_mem` member.

Key members:

| Member | Type | Purpose |
|---|---|---|
| `tsock` | `tlm_utils::simple_target_socket<SEPMemory>` | TLM bus connection |
| `m_mem` | `PagedMemory` | Sparse backing store |
| `m_read_only` | `bool` | True for ROM instances |
| `verbosity` | `csml_param<int>` | Runtime log level (CCI parameter) |
| `logger` | `CsmlLogger` | Structured logger |

Constructor signature:

```cpp
SEPMemory(sc_module_name name, bool read_only = false);
```

### 2.2 PagedMemory (`paged_mem.h`)

Pure C++ — no SystemC dependency.  4 KB page granularity.

| Behaviour | Detail |
|---|---|
| Read from unallocated page | Returns 0, no allocation |
| First write to a page | Allocates a zero-initialised 4 KB `vector<uint8_t>` |
| Page lookup | `unordered_map` → O(1) average |
| `getPtr(addr)` | Raw pointer into page for DMI; `nullptr` if unallocated |

---

## 3. TLM Interface

### 3.1 `b_transport` (blocking)

- Dispatches read → `m_mem.readBytes()`, write → `m_mem.writeBytes()`
- ROM ignores writes silently (bus transaction accepted, data discarded)
- Adds a fixed **10 ns** timing annotation per access
- Debug-logs transaction at verbosity ≥ 5

### 3.2 `get_direct_mem_ptr` (DMI)

- Returns a page-aligned DMI region for already-allocated pages only
- Unallocated pages → returns `false` (no DMI, ISS falls back to `b_transport`)
- Grants `read_write` for RW instances, `read` only for ROM

### 3.3 `transport_dbg`

- Same read/write logic as `b_transport` but no timing annotation
- ROM writes **are** permitted (debugger bypass for inspection)

---

## 4. Data Loading Interface (`load_if`)

Used by the ELF loader and platform constructor to populate memory before simulation.

| Method | Use |
|---|---|
| `load_data(src, dst_addr, n)` | Copy `n` bytes from host pointer `src` → offset `dst_addr` |
| `load_zero(dst_addr, n)` | Zero-fill `n` bytes starting at `dst_addr` (BSS init) |
| `load_binary_file(filename, addr)` | `mmap` a file and copy its bytes into the store |

---

## 5. Platform Instantiation

Four instances are created in `och_sep_ss.hpp`:

```cpp
sram = new SEPMemory("sram", false);  // read-write
rom  = new SEPMemory("rom",  true);   // read-only
itcm = new SEPMemory("itcm", false);  // read-write (ICCM)
dtcm = new SEPMemory("dtcm", false);  // read-write (DCCM)
```

Address map (from `Args.hpp` defaults):

| Instance | Base         | End          | Size   | ELF load limit |
|----------|--------------|--------------|--------|----------------|
| `sram`   | `0x10000000` | `0x1003FFFF` | 256 KB | `sram_size` (full range) |
| `rom`    | `0x10040000` | `0x1004FFFF` |  64 KB | — (pre-init via `load_data`) |
| `itcm`   | `0xC0000000` | `0xC003FFFF` | 256 KB | 128 KB (`0x20000`) |
| `dtcm`   | `0xC0040000` | `0xC005FFFF` | 128 KB |  64 KB (`0x10000`) |

Bus binding pattern (same for all four):

```cpp
bus->ports[it] = new PortMapping(opt.sram_start_addr, opt.sram_end_addr, *sram);
bus->isocks[it].bind(sram->tsock);
```

---

## 6. Key Design Decisions

**Unified module for all memory roles.**  
A single `SEPMemory` class with a `read_only` flag avoids separate SRAM/ROM/TCM implementations. ROM semantics (silent write ignore) are the only behavioural difference.

**Paged sparse storage.**  
Pages are allocated on first write. Reads from unallocated regions return 0. For typical SEP firmware (tens of kilobytes used out of hundreds allocated), this saves 80–90 % of host memory versus a flat `vector`.

**No size enforcement in the module.**  
`SEPMemory` does not hold or check a `size` field. Address bounds are enforced by the TLM bus port mapping in `och_sep_ss.hpp` — any out-of-range access never reaches the target socket.

**Fixed 10 ns timing annotation.**  
TLM timing-approximate model: one constant delay per transaction regardless of access width, page state, or burst length. Sufficient for functional simulation; not cycle-accurate.

**DMI limited to allocated pages.**  
`get_direct_mem_ptr` returns page-granular pointers only for pages that exist in the map. Unallocated accesses fall back to `b_transport`, which allocates on write.

---

## 7. Related Files

| File | Role |
|---|---|
| `sep/utils/paged-memory/paged_mem.h` | PagedMemory implementation |
| `sep/peripherals/sep_memory/model/inc/sep_memory.h` | SEPMemory declaration |
| `sep/peripherals/sep_memory/model/src/sep_memory.cpp` | SEPMemory implementation |
| `riscv-vp-plusplus/vp/src/platform/sep/och_sep_ss.hpp` | Platform instantiation and bus wiring |
| `riscv-vp-plusplus/vp/src/platform/sep/Args.hpp` | Address map defaults |
| `riscv-vp-plusplus/vp/src/platform/sep/accellera_config.ini` | Runtime verbosity overrides |
