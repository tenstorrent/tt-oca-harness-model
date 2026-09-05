# Memory Components Documentation Index

This directory (`sep/utils/paged-memory/`) and the `sep/peripherals/sep_memory/` peripheral together implement the SEP platform memory subsystem.

---

## Source Layout

```
sep/utils/paged-memory/
├── paged_mem.h                         ← PagedMemory: header-only sparse storage engine
├── MEMORY_DOCUMENTATION_INDEX.md       ← This file
└── docs/                               ← Supplementary design docs and diagrams

sep/peripherals/sep_memory/
├── model/inc/sep_memory.h              ← SEPMemory: SystemC TLM sc_module (SRAM / ROM / TCM)
├── model/src/sep_memory.cpp
├── README.md                           ← Technical overview and integration notes
└── docs/design-docs/sram-high-level-design.md ← Architecture, interfaces, platform wiring
```

---

## Components

### 1. PagedMemory (`sep/utils/paged_mem.h`)

Header-only pure-C++ sparse memory engine.  Used by `SEPMemory` as the physical storage backing.

**Design:**
- 4 KB page granularity (`PAGE_SIZE = 4096`)
- `std::unordered_map<size_t, Page>` — O(1) average page lookup
- Lazy allocation: pages are created on first write; unallocated reads return 0
- No SystemC dependency — usable in unit tests without a simulator

**API summary:**

| Method | Description |
|---|---|
| `read(addr)` | Single-byte read; returns 0 for unallocated pages |
| `write(addr, val)` | Single-byte write; allocates page on first access |
| `readBytes(addr, ptr, len)` | Bulk read into caller buffer |
| `writeBytes(addr, ptr, len)` | Bulk write from caller buffer |
| `getPtr(addr)` | Raw pointer into page (for DMI); returns `nullptr` if page not allocated |
| `getAllocatedPageCount()` | Number of currently allocated 4 KB pages |
| `clear()` | Release all pages |

---

### 2. SEPMemory (`sep/peripherals/sep_memory/`)

SystemC TLM2.0 sc_module wrapping `PagedMemory`.  A single class covers SRAM, ROM, ITCM, and DTCM — the `read_only` constructor flag enables ROM semantics.

```cpp
SEPMemory(sc_module_name name, bool read_only = false);
```

**TLM interfaces registered on `tsock`:**

| Interface | Behaviour |
|---|---|
| `b_transport` | RW access; ROM ignores writes silently; adds 10 ns timing annotation |
| `get_direct_mem_ptr` | DMI for allocated pages only; grants page-aligned range |
| `transport_dbg` | Debug access; no timing; ROM writes allowed (debugger bypass) |

**Data loading (`load_if`):**

| Method | Usage |
|---|---|
| `load_data(src, dst_addr, n)` | Load raw bytes at `dst_addr` |
| `load_zero(dst_addr, n)` | Zero-fill region |
| `load_binary_file(filename, addr)` | mmap a file and write its bytes starting at `addr` |

**Logging:** `RegLogger` with `verbosity` CCI parameter (default controlled by `REG_DEFAULT_VERBOSITY`). Runtime override via `.ini`:
```ini
och_sep_ss1.sram.verbosity : 1
```

---

## Platform Instantiation (`och_sep_ss.hpp`)

Four `SEPMemory` instances are created in `och_sep_ss::och_sep_ss()`:

```cpp
sram = new SEPMemory("sram", false);   // read-write
rom  = new SEPMemory("rom",  true);    // read-only
itcm = new SEPMemory("itcm", false);   // read-write (ICCM)
dtcm = new SEPMemory("dtcm", false);   // read-write (DCCM)
```

### Address Map (from `Args.hpp` defaults)

| Instance | Start        | End          | Size   | Bus binding call |
|----------|--------------|--------------|--------|------------------|
| `sram`   | `0x10000000` | `0x1003FFFF` | 256 KB | `bus->isocks[it++].bind(sram->tsock)` |
| `rom`    | `0x10040000` | `0x1004FFFF` |  64 KB | `bus->isocks[it++].bind(rom->tsock)` |
| `itcm`   | `0xC0000000` | `0xC003FFFF` | 256 KB | `bus->isocks[it++].bind(itcm->tsock)` |
| `dtcm`   | `0xC0040000` | `0xC005FFFF` | 128 KB | `bus->isocks[it++].bind(dtcm->tsock)` |

### ELF Loading

```cpp
loader.load_executable_image(*itcm, 0x20000, opt.itcm_start_addr);  // .text → ITCM (128 KB limit)
loader.load_executable_image(*dtcm, 0x10000, opt.dtcm_start_addr);  // .data → DTCM  (64 KB limit)
loader.load_executable_image(*sram, opt.sram_size, opt.sram_start_addr);
```

ROM is pre-initialised at construction for the `rom_sanity_test` firmware via `rom->load_data()`.

---

## Architecture Diagram

```
     VeeR ISS (RISC-V core)
           │  TLM
    ┌──────┴──────┐
    │  System Bus │
    └──┬──┬──┬───┘
       │  │  │  └────────────────────────┐
       │  │  └────────────────────┐      │
       │  └──────────────┐        │      │
       │                 │        │        │
  ┌────┴────┐      ┌─────┴──┐  ┌──┴───┐ ┌──┴───┐
  │  sram   │      │  rom   │  │ itcm │ │ dtcm │
  │  256 KB │      │  64 KB │  │256 KB│ │128 KB│
  │  RW     │      │  RO    │  │  RW  │ │  RW  │
  └────┬────┘      └─────┬──┘  └──┬───┘ └──┬───┘
       └──────────────────┴────────┴─────────┘
                          │
                   SEPMemory (sc_module)
                          │
                   PagedMemory (C++)
                          │
             unordered_map<addr, 4KB page>
```

---

## Memory Efficiency

Pages are allocated only on first write.  For a 256 KB SRAM with 32 KB of actual code/data:

| Metric | Value |
|---|---|
| Address space | 256 KB |
| Pages allocated | 8 × 4 KB = 32 KB |
| Host memory used | 32 KB |
| Savings vs. flat allocation | 87.5% |

---

## Related Files

| File | Purpose |
|---|---|
| `sep/utils/paged-memory/paged_mem.h` | PagedMemory implementation |
| `sep/peripherals/sep_memory/model/inc/sep_memory.h` | SEPMemory class declaration |
| `sep/peripherals/sep_memory/model/src/sep_memory.cpp` | SEPMemory implementation |
| `sep/peripherals/sep_memory/README.md` | Technical overview |
| `sep/peripherals/sep_memory/docs/design-docs/sram-high-level-design.md` | Architecture and platform wiring |
| `riscv-vp-plusplus/vp/src/platform/sep/och_sep_ss.hpp` | Platform instantiation |
| `riscv-vp-plusplus/vp/src/platform/sep/Args.hpp` | Address map defaults |
| `riscv-vp-plusplus/vp/src/platform/sep/accellera_config.ini` | Runtime verbosity config |
