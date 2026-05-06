# OCH SEP Common Headers

This directory contains common header files for OCH SEP firmware and tests.

## Files

### `och_sep_common.h`

Provides register access macros similar to tt_sep's `sep_common.h`.

**Key Macros:**

| Macro | Description | Example |
|-------|-------------|---------|
| `WRITE_REG(addr, value)` | Write 32-bit register | `WRITE_REG(OTBN_CMD_REG_ADDR, cmd)` |
| `READ_REG(addr)` | Read 32-bit register | `status = READ_REG(OTBN_STATUS_REG_ADDR)` |
| `WRITE_REG64(addr, value)` | Write 64-bit register | `WRITE_REG64(reg_addr, value64)` |
| `READ_REG64(addr)` | Read 64-bit register | `value = READ_REG64(reg_addr)` |
| `WRITE_MEM_WORD(base, idx, val)` | Write memory word | `WRITE_MEM_WORD(DMEM_BASE, 0, data)` |
| `READ_MEM_WORD(base, idx)` | Read memory word | `data = READ_MEM_WORD(DMEM_BASE, 0)` |
| `SET_BITS(addr, mask)` | Set bits (RMW) | `SET_BITS(reg_addr, 0x3)` |
| `CLEAR_BITS(addr, mask)` | Clear bits (RMW) | `CLEAR_BITS(reg_addr, 0x3)` |

## Comparison with tt_sep

### tt_sep (WRITE_EXT/READ_EXT)
```c
#define SEP_EXT_BASE (0xc0000000)
#define WRITE_EXT(addr, value) \
  (*((volatile uint32_t *)(uintptr_t)(SEP_EXT_BASE + addr)) = value)
#define READ_EXT(addr) \
  (*((volatile uint32_t *)(uintptr_t)(SEP_EXT_BASE + addr)))

// Usage - addr is an OFFSET from SEP_EXT_BASE
WRITE_EXT(0x180_0000, data);  // Accesses 0xc180_0000
```

### och_sep (WRITE_REG/READ_REG)
```c
// No base offset - addresses are already absolute
#define WRITE_REG(addr, value) \
  (*((volatile uint32_t *)(uintptr_t)(addr)) = value)
#define READ_REG(addr) \
  (*((volatile uint32_t *)(uintptr_t)(addr)))

// Usage - addr is ABSOLUTE (from och_sep_top_reg.h)
WRITE_REG(0x40000010, data);  // Direct absolute address
// Or more commonly:
WRITE_REG(OTBN_CMD_REG_ADDR, data);  // OTBN_CMD_REG_ADDR = 0x40000010
```

## Usage Example

```c
#include "och_sep_top_reg.h"  // Get register addresses
#include "och_sep_common.h"   // Get access macros

void example(void) {
    // Write command register
    WRITE_REG(OTBN_CMD_REG_ADDR, OTBN_CMD_EXECUTE);

    // Poll status register
    while (READ_REG(OTBN_STATUS_REG_ADDR) == OTBN_STATUS_BUSY);

    // Check error register
    uint32_t err = READ_REG(OTBN_ERR_BITS_REG_ADDR);

    // Write to memory
    WRITE_MEM_WORD(OTBN_IMEM_MEM_BASE_ADDR, 0, instruction);

    // Read from memory
    uint32_t data = READ_MEM_WORD(OTBN_DMEM_MEM_BASE_ADDR, 10);
}
```

## Why No Base Offset?

In tt_sep, registers require adding `SEP_EXT_BASE` (0xc0000000) due to SEP's address translation.

In och_sep, `och_sep_top_reg.h` already provides **absolute addresses**:
- `OTBN_CMD_REG_ADDR` = `0x40000010` (absolute, not offset)
- `HMAC_CFG_REG_ADDR` = `0x40088010` (absolute, not offset)

Therefore, `WRITE_REG`/`READ_REG` use addresses directly without adding any offset.

## Benefits

1. ✅ **Consistent API** - Similar to tt_sep's WRITE_EXT/READ_EXT
2. ✅ **Clean code** - `WRITE_REG(addr, val)` vs `*((uint32_t*)addr) = val`
3. ✅ **Type safety** - Casts handled by macro
4. ✅ **Easy migration** - Familiar pattern from tt_sep
5. ✅ **Read-modify-write** - SET_BITS, CLEAR_BITS helpers

## See Also

- `../../../meta/registers/README.md` - Register generation infrastructure
- `../../../meta/registers/example_usage.c` - More examples
