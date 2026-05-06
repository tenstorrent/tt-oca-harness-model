# Paged Memory Programmer's Guide

## Overview

This guide provides practical information for using the PagedMemory class in your SystemC TLM models. The PagedMemory class provides sparse memory allocation with on-demand page creation.

## Quick Start

### Basic Usage

```cpp
#include "paged_mem.h"

// Create paged memory instance
PagedMemory memory;

// Write single byte
memory.write(0x1000, 0x42);

// Read single byte
uint8_t value = memory.read(0x1000);  // Returns 0x42

// Read from unallocated region
uint8_t zero = memory.read(0x5000);   // Returns 0 (no allocation)
```

### Multi-Byte Operations

```cpp
// Write multiple bytes from buffer
uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
memory.writeBytes(0x2000, data, sizeof(data));

// Write from std::vector
std::vector<uint8_t> vec_data = {0xAA, 0xBB, 0xCC};
memory.writeBytes(0x3000, vec_data);

// Read multiple bytes
std::vector<uint8_t> result = memory.readBytes(0x2000, 4);
// result = {0x01, 0x02, 0x03, 0x04}
```

## Integration Patterns

### Pattern 1: SRAM Memory Model

```cpp
class SRAM : public sc_module {
    PagedMemory m_sram_data;
    uint64_t m_sram_sz;
    
public:
    SRAM(sc_module_name name, uint64_t size)
        : sc_module(name), m_sram_sz(size) {}
    
    unsigned transport_dbg(tlm::tlm_generic_payload &trans) {
        tlm::tlm_command cmd = trans.get_command();
        unsigned addr = trans.get_address();
        auto *ptr = trans.get_data_ptr();
        auto len = trans.get_data_length();
        
        assert(addr < m_sram_sz);  // Bounds check
        
        if (cmd == tlm::TLM_WRITE_COMMAND) {
            m_sram_data.writeBytes(addr, ptr, len);
        } else if (cmd == tlm::TLM_READ_COMMAND) {
            auto data = m_sram_data.readBytes(addr, len);
            memcpy(ptr, data.data(), len);
        }
        
        return len;
    }
};
```

### Pattern 2: ROM Memory Model

```cpp
class ROM : public sc_module {
    PagedMemory m_rom_data;
    uint64_t m_rom_sz;
    
public:
    ROM(sc_module_name name, uint64_t size) 
        : sc_module(name), m_rom_sz(size) {}
    
    // Loading interface (allowed for initialization)
    void load_data(const char *src, uint64_t addr, size_t n) {
        assert(addr + n <= m_rom_sz);
        m_rom_data.writeBytes(addr, (const uint8_t*)src, n);
    }
    
    // TLM interface (read-only)
    unsigned transport_dbg(tlm::tlm_generic_payload &trans) {
        tlm::tlm_command cmd = trans.get_command();
        
        if (cmd == tlm::TLM_WRITE_COMMAND) {
            sc_assert(false && "ROM write not supported");
        }
        
        // Read operations allowed
        auto addr = trans.get_address();
        auto len = trans.get_data_length();
        auto data = m_rom_data.readBytes(addr, len);
        memcpy(trans.get_data_ptr(), data.data(), len);
        
        return len;
    }
};
```

### Pattern 3: ELF Binary Loading

```cpp
class MemoryLoader {
    PagedMemory &memory;
    
public:
    void load_elf_segment(const char *data, uint64_t vaddr, size_t size) {
        // Load .text, .data sections
        memory.writeBytes(vaddr, (const uint8_t*)data, size);
    }
    
    void zero_bss_section(uint64_t vaddr, size_t size) {
        // Zero-fill .bss section
        std::vector<uint8_t> zeros(size, 0);
        memory.writeBytes(vaddr, zeros);
    }
};
```

## Best Practices

### 1. Bounds Checking

Always validate address ranges before accessing PagedMemory:

```cpp
void access_memory(uint64_t addr, size_t len) {
    // ✅ Good: Check bounds
    if (addr + len > m_memory_size) {
        cerr << "Access out of bounds!" << endl;
        return;
    }
    
    auto data = memory.readBytes(addr, len);
}
```

### 2. Efficient Bulk Operations

Use multi-byte operations instead of loops:

```cpp
// ❌ Bad: Inefficient byte-by-byte
for (size_t i = 0; i < 1000; i++) {
    memory.write(base_addr + i, data[i]);
}

// ✅ Good: Single bulk operation 
memory.writeBytes(base_addr, data, 1000);
```

**Note**: Internally, `writeBytes()` still loops byte-by-byte, but it's clearer and allows future optimization.

### 3. Memory Profiling

Track allocation to understand memory usage:

```cpp
// Before loading
size_t pages_before = memory.getAllocatedPageCount();

// Load program
loader.load_executable(memory, program_data);

// After loading
size_t pages_after = memory.getAllocatedPageCount();
size_t pages_allocated = pages_after - pages_before;
size_t kb_used = pages_allocated * 4;

cout << "Program loaded: " << kb_used << " KB (" 
     << pages_allocated << " pages)" << endl;
```

### 4. Pointer Access (Advanced)

For performance-critical code, use direct pointers:

```cpp
uint8_t* ptr = memory.getPtr(address);

if (ptr != nullptr) {
    // Page is allocated, can use pointer directly
    *ptr = 0x42;
} else {
    // Page not allocated
    // Must use write() to allocate first
    memory.write(address, 0x42);
}
```

**⚠️ Warning**: Pointers are only valid for bytes within the same 4KB page. Do not use pointer arithmetic across page boundaries.

## Common Use Cases

### Use Case 1: Boot ROM Initialization

```cpp
ROM boot_rom("boot_rom", 0x10000);  // 64KB ROM

// Load boot code at reset vector
const char boot_code[] = {
    0x13, 0x01, 0x01, 0xFF,  // addi sp, sp, -16
    0x23, 0x26, 0x11, 0x00,  // sw ra, 12(sp)
    // ... more boot code
};

boot_rom.load_data(boot_code, 0x00000000, sizeof(boot_code));

// Load configuration at high address
uint32_t config_word = 0x12345678;
boot_rom.load_data((char*)&config_word, 0x0000FFC, sizeof(config_word));

// Check allocation
cout << "ROM pages allocated: " 
     << boot_rom.m_rom_data.getAllocatedPageCount() << endl;
// Output: 2 pages (page 0 for boot code, page 15 for config)
```

### Use Case 2: Sparse Data Structure

```cpp
PagedMemory sparse_array;

// Write to widely-spaced addresses
sparse_array.write(0x00000, 0x01);  // Allocates page 0
sparse_array.write(0x10000, 0x02);  // Allocates page 16
sparse_array.write(0x20000, 0x03);  // Allocates page 32

// Only 3 pages allocated (12KB), not 128KB!
assert(sparse_array.getAllocatedPageCount() == 3);
```

### Use Case 3: Memory Clearing

```cpp
// Clear all allocated pages
memory.clear();

// Now all pages deallocated
assert(memory.getAllocatedPageCount() == 0);

// Reads return zero (sparse representation restored)
uint8_t value = memory.read(0x1000);  // Returns 0
```

## Performance Tips

### 1. Minimize Cross-Page Access

```cpp
// ✅ Good: Aligned to page boundary
uint64_t page_aligned_addr = 0x00001000;
memory.writeBytes(page_aligned_addr, data, 4096);
// Single page accessed

// ⚠️ Less efficient: Crosses page boundary
uint64_t unaligned_addr = 0x00000FFE;
memory.writeBytes(unaligned_addr, data, 4);
// Two pages accessed (0x00000000 and 0x00001000)
```

### 2. Sequential Access Patterns

Sequential access within a page is cache-friendly:

```cpp
// ✅ Good: Sequential within page
for (int i = 0; i < 1000; i++) {
    data[i] = memory.read(0x1000 + i);
}

// ⚠️ Poor: Random access pattern
for (int i = 0; i < 1000; i++) {
    data[i] = memory.read(random_address());
}
```

### 3. Pre-allocation for Known Patterns

If you know pages will be used, write to them early:

```cpp
// Pre-allocate pages for known memory regions
void preallocate_memory_regions() {
    // Allocate .text region
    memory.write(0x10000, 0);  // First byte of page
    
    // Allocate .data region  
    memory.write(0x20000, 0);  // First byte of page
    
    // Allocate stack region
    memory.write(0x30000, 0);  // First byte of page
}
```

## Debugging Tips

### Visualization

```cpp
void print_memory_map(const PagedMemory &mem) {
    size_t page_count = mem.getAllocatedPageCount();
    
    cout << "Memory Map:" << endl;
    cout << "  Allocated pages: " << page_count << endl;
    cout << "  Memory used: " << (page_count * 4) << " KB" << endl;
    
    // Note: Cannot iterate pages (private member)
    // This is a limitation of current API
}
```

### Checking Allocation

```cpp
bool is_page_allocated(PagedMemory &mem, uint64_t addr) {
    uint8_t *ptr = mem.getPtr(addr);
    return (ptr != nullptr);
}

// Usage
if (is_page_allocated(memory, 0x5000)) {
    cout << "Page at 0x5000 is allocated" << endl;
} else {
    cout << "Page at 0x5000 is NOT allocated" << endl;
}
```

## Thread Safety

**⚠️ Important**: PagedMemory is **NOT thread-safe**. 

For SystemC TLM models, this is acceptable because:
- SystemC uses cooperative scheduling (no preemption)
- TLM transactions are atomic from model perspective
- No concurrent access within simulation time

If you need thread-safe access:

```cpp
class ThreadSafePagedMemory {
    PagedMemory memory;
    std::mutex mtx;
    
public:
    uint8_t read(size_t addr) {
        std::lock_guard<std::mutex> lock(mtx);
        return memory.read(addr);
    }
    
    void write(size_t addr, uint8_t value) {
        std::lock_guard<std::mutex> lock(mtx);
        memory.write(addr, value);
    }
};
```

## API Reference Summary

| Method | Description | Allocates? |
|--------|-------------|------------|
| `read(addr)` | Read single byte | No |
| `write(addr, value)` | Write single byte | Yes (if needed) |
| `readBytes(addr, len)` | Read multiple bytes | No |
| `writeBytes(addr, data, len)` | Write multiple bytes | Yes (if needed) |
| `getPtr(addr)` | Get direct pointer | No |
| `getAllocatedPageCount()` | Get page count | No |
| `clear()` | Deallocate all pages | No |

For complete API documentation, see [Paged Memory Specification](paged-memory-specification.md).

## Related Documentation

- [Theory of Operation](theory_of_operation.md) - Implementation details and design rationale
- [SRAM Usage Example](../../sram-knowledge-base/README.md) - SRAM integration
- [ROM Usage Example](../../../rom/rom-knowledge-base/README.md) - ROM integration
