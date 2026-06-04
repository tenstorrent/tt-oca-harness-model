# sep_memory

SystemC TLM2.0 memory module for the SEP virtual platform.  A single `SEPMemory` class covers all four platform memory regions — SRAM, ROM, ITCM, and DTCM — differentiated only by the `read_only` constructor flag.

## Files

```
model/inc/sep_memory.h      SEPMemory class declaration
model/src/sep_memory.cpp    SEPMemory implementation
```

Storage is provided by `PagedMemory` from `sep/utils/paged-memory/paged_mem.h` — a header-only pure-C++ sparse engine that lazily allocates 4 KB pages.

## Instantiation

```cpp
SEPMemory(sc_module_name name, bool read_only = false);

// Platform usage (och_sep_ss.hpp):
sram = new SEPMemory("sram", false);  // RW  — 0x10000000–0x1003FFFF (256 KB)
rom  = new SEPMemory("rom",  true);   // RO  — 0x10040000–0x1004FFFF  (64 KB)
itcm = new SEPMemory("itcm", false);  // RW  — 0xC0000000–0xC003FFFF (256 KB)
dtcm = new SEPMemory("dtcm", false);  // RW  — 0xC0040000–0xC005FFFF (128 KB)
```

## Interfaces

| Interface | Notes |
|---|---|
| `tsock` (`simple_target_socket`) | TLM2.0 bus connection |
| `b_transport` | Read/write; ROM ignores writes; +10 ns timing annotation |
| `get_direct_mem_ptr` | Per-page DMI for allocated pages only |
| `transport_dbg` | Debug access, no timing, ROM writes permitted |
| `load_data(src, addr, n)` | Bulk load from host pointer (ELF loader) |
| `load_zero(addr, n)` | Zero-fill region (BSS init) |
| `load_binary_file(filename, addr)` | mmap a file into the store |

## Logging

CSML logger with a `verbosity` CCI parameter. Override at runtime via `accellera_config.ini`:

```ini
och_sep_ss1.sram.verbosity : 1
och_sep_ss1.rom.verbosity  : 1
och_sep_ss1.itcm.verbosity : 1
och_sep_ss1.dtcm.verbosity : 1
```

## Documentation

- [High-Level Design](docs/design-docs/sram-high-level-design.md) — architecture, TLM interfaces, platform wiring, design decisions
