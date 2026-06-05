# Paged Memory Interfaces

## C++ Class Interface

The PagedMemory class provides a clean C++ interface for sparse memory management. It is implemented as a standalone utility class without dependencies on SystemC or TLM libraries.

### Class Declaration

```cpp
class PagedMemory {
public:
    // Constructor and Destructor
    PagedMemory() = default;
    ~PagedMemory() = default;
    
    // Single-byte access
    uint8_t read(size_t address);
    void write(size_t address, uint8_t value);
    
    // Multi-byte access  
    std::vector<uint8_t> readBytes(size_t address, size_t length);
    void writeBytes(size_t address, const uint8_t* data, size_t length);
    void writeBytes(size_t address, const std::vector<uint8_t>& data);
    
    // Advanced access
    uint8_t* getPtr(size_t address);
    
    // Utilities
    size_t getAllocatedPageCount() const;
    void clear();
    
private:
    std::unordered_map<size_t, Page> memory_map;
};

using Page = std::vector<uint8_t>;
```

## Method Descriptions

### Constructor/Destructor

**`PagedMemory()`**
- Default constructor
- Initializes empty memory (no pages allocated)
- No parameters required

**`~PagedMemory()`**
- Default destructor  
- Automatically deallocates all pages via `std::unordered_map` destructor
- No explicit cleanup required

### Single-Byte Access

**`uint8_t read(size_t address)`**
- **Purpose**: Read single byte from memory
- **Parameters**:
  - `address`: Byte address to read from
- **Returns**: Byte value (actual data if page allocated, 0 if not)
- **Side Effects**: None (no allocation on read)
- **Complexity**: O(1) average case

**`void write(size_t address, uint8_t value)`**
- **Purpose**: Write single byte to memory
- **Parameters**:
  - `address`: Byte address to write to
  - `value`: 8-bit value to write
- **Returns**: void
- **Side Effects**: Allocates 4KB page if address not yet allocated
- **Complexity**: O(1) average, O(PAGE_SIZE) worst case on first write to page

### Multi-Byte Access

**`std::vector<uint8_t> readBytes(size_t address, size_t length)`**
- **Purpose**: Read multiple consecutive bytes
- **Parameters**:
  - `address`: Starting byte address
  - `length`: Number of bytes to read
- **Returns**: `std::vector` containing read data
- **Side Effects**: None (no allocation)
- **Complexity**: O(length)
- **Page Boundaries**: Automatically handles reads crossing page boundaries

**`void writeBytes(size_t address, const uint8_t* data, size_t length)`**
- **Purpose**: Write multiple consecutive bytes from C-style array
- **Parameters**:
  - `address`: Starting byte address
  - `data`: Pointer to source data
  - `length`: Number of bytes to write
- **Returns**: void
- **Side Effects**: Allocates pages as needed for entire range
- **Complexity**: O(length)

**`void writeBytes(size_t address, const std::vector<uint8_t>& data)`**
- **Purpose**: Write multiple consecutive bytes from `std::vector`
- **Parameters**:
  - `address`: Starting byte address
  - `data`: Source vector (size determines write length)
- **Returns**: void
- **Side Effects**: Allocates pages as needed
- **Complexity**: O(data.size())

### Advanced Access

**`uint8_t* getPtr(size_t address)`**
- **Purpose**: Get direct pointer to byte in memory
- **Parameters**:
  - `address`: Byte address
- **Returns**: Pointer to byte if page allocated, `nullptr` otherwise
- **Side Effects**: None (doesn't allocate)
- **Warnings**: 
  - Pointer only valid within 4KB page containing address
  - Do not use pointer arithmetic across page boundaries
  - Pointer may be invalidated if map resizes
- **Use Case**: Performance-critical code with controlled access patterns

### Utility Methods

**`size_t getAllocatedPageCount() const`**
- **Purpose**: Get number of currently allocated pages
- **Parameters**: None
- **Returns**: Number of allocated 4KB pages
- **Side Effects**: None
- **Use Case**: Memory profiling and diagnostics

**`void clear()`**
- **Purpose**: Deallocate all pages, reset to empty state
- **Parameters**: None
- **Returns**: void
- **Side Effects**: All pages deallocated, allocated count becomes 0
- **Post-condition**: Subsequent reads return 0 (until new writes)

## Helper Functions

**`size_t getPageBase(size_t address)`** (inline, not class member)
- **Purpose**: Calculate page base address from byte address
- **Formula**: `address & ~(PAGE_SIZE - 1)`
- **Example**: `getPageBase(0x12ABC) = 0x12000`

## Constants

**`constexpr size_t PAGE_SIZE = 4096`**
- Defines page size (4KB)
- Used for page alignment calculations
- Changing requires recompilation

## Type Aliases

**`using Page = std::vector<uint8_t>`**
- Each page is a vector of 4096 bytes
- Zero-initialized on allocation
- Provides contiguous storage within page

## Integration Interface Patterns

### Pattern 1: TLM-UL Target Memory

```cpp
class TLM_Memory {
    PagedMemory storage;
    
    void b_transport(tlm::tlm_generic_payload &trans, sc_time &delay) {
        if (trans.get_command() == TLM_READ_COMMAND) {
            auto data = storage.readBytes(trans.get_address(), 
                                          trans.get_data_length());
            memcpy(trans.get_data_ptr(), data.data(), data.size());
        }
        else if (trans.get_command() == TLM_WRITE_COMMAND) {
            storage.writeBytes(trans.get_address(),
                              trans.get_data_ptr(),
                              trans.get_data_length());
        }
        delay += sc_time(10, SC_NS);
    }
};
```

### Pattern 2: ELF Loader Interface

```cpp
class ELF_LoadableMemory : public load_if {
    PagedMemory storage;
    
    void load_data(const char *src, uint64_t dst_addr, size_t n) override {
        storage.writeBytes(dst_addr, (const uint8_t*)src, n);
    }
    
    void load_zero(uint64_t dst_addr, size_t n) override {
        std::vector<uint8_t> zeros(n, 0);
        storage.writeBytes(dst_addr, zeros);
    }
};
```

### Pattern 3: Debug Access Interface

```cpp
class DebugMemoryInterface {
    PagedMemory storage;
    
    unsigned transport_dbg(tlm::tlm_generic_payload &trans) {
        // Zero-time access for debugger
        // (Implementation similar to b_transport but no timing)
    }
};
```

## Memory Layout

### Logical View

```
Address Space (arbitrary size, e.g., 4GB):
┌─────────────────────────────────────┐
│  0x00000000 - 0xFFFFFFFF            │
│  (Not all allocated)                │
└─────────────────────────────────────┘
```

### Physical Allocation

```
Allocated Pages (sparse, in hash map):
┌──────────────┐  ┌──────────────┐  ┌──────────────┐
│ Page @0x0000 │  │ Page @0x5000 │  │ Page @0xA000 │
│ 4096 bytes   │  │ 4096 bytes   │  │ 4096 bytes   │
└──────────────┘  └──────────────┘  └──────────────┘

Unallocated regions (implicit):
0x1000-0x4FFF, 0x6000-0x9FFF, 0xB000+  (return 0 on read)
```

## Performance Characteristics

| Operation | Best Case | Average Case | Worst Case |
|-----------|-----------|--------------|------------|
| `read()` allocated | O(1) | O(1) | O(n) * |
| `read()` unallocated | O(1) | O(1) | O(n) * |
| `write()` allocated | O(1) | O(1) | O(n) * |
| `write()` unallocated | O(1) | O(1) | O(n) * + O(PAGE_SIZE) |
| `readBytes(n)` | O(n) | O(n) | O(n²) * |
| `writeBytes(n)` | O(n) | O(n) | O(n²) * |
| `getPtr()` | O(1) | O(1) | O(n) * |
| `getAllocatedPageCount()` | O(1) | O(1) | O(1) |
| `clear()` | O(n) | O(n) | O(n) |

\* Hash map worst case depends on hash collisions, where n = number of allocated pages

## Thread Safety

**⚠️ NOT THREAD-SAFE**

The PagedMemory class is not thread-safe:
- No internal locking/synchronization
- Concurrent access from multiple threads is undefined behavior
- Suitable for single-threaded SystemC simulation environment

## Dependencies

**Required Headers**:
```cpp
#include <unordered_map>  // For page hash map
#include <vector>         // For page storage
#include <memory>         // For smart pointers (if needed)
#include <cstring>        // For memcpy operations
```

**Standard**: C++11 or later

## Error Handling

The PagedMemory class uses **assertions** rather than exceptions:

- No out-of-bounds checking (caller responsible)
- No null pointer checks (caller responsible)
- Assertions may be used in debug builds

**Best Practice**: Wrapper classes (SRAM, ROM) should validate addresses before calling PagedMemory methods.

## Comparison with Standard Containers

| Feature | PagedMemory | std::vector | std::map<size_t, uint8_t> |
|---------|------------|-------------|---------------------------|
| Random access | O(1) avg | O(1) | O(log n) |
| Memory model | Sparse pages | Contiguous | Per-byte entries |
| Space efficiency | High (sparse) | Low (dense) | Very low (overhead) |
| Cache locality | Good (within page) | Excellent | Poor |
| Suitable for | Large sparse memory | Small dense array | Tiny sparse data |

## Related Interfaces

- **load_if**: ELF loader interface (implemented by SRAM/ROM)
- **tlm_target_socket**: TLM memory-mapped interface
- **tlm_generic_payload**: TLM transaction descriptor

## Example Usage Scenarios

### Scenario 1: Reading Uninitialized Memory

```cpp
PagedMemory mem;
// No writes yet

uint8_t value = mem.read(0x1000);
// Returns: 0 (page not allocated)

size_t pages = mem.getAllocatedPageCount();
// Returns: 0 (no pages allocated)
```

### Scenario 2: Sparse Writes

```cpp
PagedMemory mem;

mem.write(0x0000, 0xAA);  // Allocates page 0
mem.write(0x5000, 0xBB);  // Allocates page 5

size_t pages = mem.getAllocatedPageCount();
// Returns: 2 (only 2 pages for 20KB+ address range)
```

### Scenario 3: Page Boundary Crossing

```cpp
PagedMemory mem;

uint8_t data[4] = {0x01, 0x02, 0x03, 0x04};
mem.writeBytes(0x0FFE, data, 4);
// Writes:
//   0x0FFE → 0x01 (page 0)
//   0x0FFF → 0x02 (page 0)
//   0x1000 → 0x03 (page 1) - new page!
//   0x1001 → 0x04 (page 1)

size_t pages = mem.getAllocatedPageCount();
// Returns: 2 (crosses boundary, allocates both pages)
```

## Documentation References

- [Paged Memory API Reference](paged-memory-specification.md) - Complete API documentation
- [Theory of Operation](theory_of_operation.md) - Design details and rationale
- [Programmer's Guide](programmers_guide.md) - Usage patterns and best practices
