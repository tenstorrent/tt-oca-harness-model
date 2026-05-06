# Memory Components Documentation Index

This directory contains comprehensive design documentation for the memory subsystem components used in the RISC-V VP++ SEP platform.

## Components

### 1. SRAM (Static Random-Access Memory)
**Location**: `models/generic/sram/`

Read-write memory with paged architecture for efficient sparse allocation.

**Documentation**:
- [Knowledge Base README](sram/sram-knowledge-base/README.md) - Technical specifications and overview
- [High-Level Design](sram/sram-generated-docs/sram-high-level-design.md) - Architecture, features, and TLM implementation
- [Test Plan](sram/sram-generated-docs/sram-test-plan.md) - Test cases and validation status

**Key Features**:
- 4KB paged memory architecture
- On-demand page allocation
- Byte-addressable read/write access
- TLM2.0 compliance
- ELF binary loading support

---

### 2. ROM (Read-Only Memory)
**Location**: `models/generic/rom/`

Write-protected memory using the same paged architecture as SRAM.

**Documentation**:
- [Knowledge Base README](rom/rom-knowledge-base/README.md) - Technical specifications and overview
- [High-Level Design](rom/rom-generated-docs/rom-high-level-design.md) - Architecture and write protection mechanisms
- [Test Plan](rom/rom-generated-docs/rom-test-plan.md) - Test cases and validation status

**Key Features**:
- Read-only TLM access with write protection
- Same paged architecture as SRAM
- Boot code and constant data storage
- load_if interface for initialization

---

### 3. Paged Memory
**Location**: `models/generic/sram/utils/`

Core utility class providing sparse memory allocation for both SRAM and ROM.

**Documentation**:
- [Technical Specification](sram/utils/paged-memory-docs/paged-memory-specification.md) - API reference and implementation details

**Key Features**:
- Hash-based page lookup (O(1) average case)
- Zero-default reads for unallocated pages
- Lazy allocation on first write
- Dramatic memory savings (80-95%) for sparse workloads

---

## Documentation Structure

Each component follows a consistent documentation structure inspired by the HMAC IP documentation:

```
component-name/
├── component-knowledge-base/
│   └── README.md                    # Overview and features
├── component-generated-docs/
│   ├── component-high-level-design.md   # Architecture and design
│   └── component-test-plan.md           # Test cases and validation
└── model/
    ├── inc/                         # Header files
    └── src/                         # Implementation
```

---

## Quick Links

### SRAM
- [SRAM Overview](sram/sram-knowledge-base/README.md)
- [SRAM Architecture](sram/sram-generated-docs/sram-high-level-design.md)
- [SRAM Tests](sram/sram-generated-docs/sram-test-plan.md)

### ROM
- [ROM Overview](rom/rom-knowledge-base/README.md)
- [ROM Architecture](rom/rom-generated-docs/rom-high-level-design.md)
- [ROM Tests](rom/rom-generated-docs/rom-test-plan.md)

### Paged Memory
- [Paged Memory Specification](sram/utils/paged-memory-docs/paged-memory-specification.md)

---

## Architecture Overview

```
┌─────────────────────────────────────────┐
│         Application Software            │
└──────────────┬──────────────────────────┘
               │
┌──────────────┴──────────────┐
│       RISC-V ISS            │
└──────────────┬──────────────┘
               │
┌──────────────┴──────────────┐
│        TLM Bus              │
└─────┬────────────────┬──────┘
      │                │
┌─────┴─────┐    ┌────┴─────┐
│   SRAM    │    │   ROM    │
│ (128 KB)  │    │  (64 KB) │
└─────┬─────┘    └────┬─────┘
      │                │
┌─────┴────────────────┴─────┐
│      PagedMemory            │
│  (Shared Implementation)    │
│                             │
│  ┌────────────────────┐    │
│  │  Page Map (hash)   │    │
│  ├────────────────────┤    │
│  │ Page 0: 4KB vector │    │
│  │ Page 1: 4KB vector │    │
│  │ ...                │    │
│  └────────────────────┘    │
└─────────────────────────────┘
```

---

## Memory Efficiency Comparison

For a 128KB SRAM + 64KB ROM configuration with typical embedded software:

| Memory Type | Address Space | Actual Usage | Traditional Model | Paged Model | Savings |
|-------------|---------------|--------------|-------------------|-------------|---------|
| SRAM | 128 KB | 24 KB code+data | 128 KB | 24 KB | 81% |
| ROM | 64 KB | 4 KB boot code | 64 KB | 4 KB | 93.75% |
| **Total** | **192 KB** | **28 KB** | **192 KB** | **28 KB** | **85.4%** |

The paged architecture provides **85% memory savings** for typical embedded workloads!

---

## Usage Example

### SRAM Integration (from main.cpp)
```cpp
// Create 128KB SRAM
SRAM sram("sram", 0x20000, false);  // false = read-write

// Load ELF binary
loader.load_executable_image(sram, sram.m_sram_sz, 0x10100000);

// Connect to bus
bus.ports[1] = new PortMapping(0x10100000, 0x1011FFFF, sram);
bus.isocks[1].bind(sram.tsock);
```

### ROM Integration (from main.cpp)
```cpp
// Create 64KB ROM
ROM rom("rom", 0x10000);

// Load boot code
rom.load_binary_file("boot.bin", 0x00000000);

// Connect to bus
bus.ports[5] = new PortMapping(0x10000000, 0x1000FFFF, rom);
bus.isocks[5].bind(rom.tsock);
```

---

## Testing

### Run SRAM Tests
```bash
cd sw/sep-vp-tests/sram_basic_test
make && make run
```

### Run ROM Tests
```bash
cd sw/sep-vp-tests/rom_test
make && make run
```

### Integration Tests
All existing platform tests validate memory subsystem:
- `uart16550_test` - Uses SRAM for code/data
- `hmac_test` - Uses SRAM for test vectors
- `rom_test` - Uses ROM for boot sequence

---

## Related Documentation

- [HMAC Documentation](../../models/ot/hmac/hmac-knowledge-base/) - Example of detailed IP documentation
- [Platform Integration](../../riscv-vp-plusplus/vp/src/platform/sep/main.cpp) - Memory subsystem integration
- [SystemC TLM2.0](https://www.systemc.org) - TLM standard documentation

---

## Document History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2025-12-04 | AI Assistant | Initial documentation creation |

---

## Future Enhancements

Potential improvements to memory subsystem:

1. **Per-Page DMI**: Support DMI for allocated pages
2. **Memory Profiling**: Track allocation patterns and access heat maps
3. **Page Compression**: Compress pages with redundant data
4. **Configurable Page Sizes**: Support 1KB, 2KB, 8KB pages
5. **Write-Through**: Optional disk backing for persistence

---

**Note**: This documentation follows the same structure as the HMAC IP documentation to maintain consistency across the project.
