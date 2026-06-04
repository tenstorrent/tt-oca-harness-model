# OpenTitan Compatibility Layer for SEP Platform

This directory contains a minimal compatibility layer that allows OpenTitan test code to be compiled and run on the SEP RISC-V VP++ platform.

## Purpose

OpenTitan uses a comprehensive software framework (OTTF) and Device Interface Functions (DIF) libraries. This compatibility layer provides a minimal subset of these APIs, allowing OpenTitan tests to be ported to the SEP platform without requiring the full OpenTitan build system.

## Components

### Core Headers

- **`base_compat.h`** - Basic types and macros
  - `dif_result_t` - Result type for DIF functions
  - `mmio_region_t` - MMIO region abstraction
  - Register access macros

- **`ottf_compat.h`** - Test framework compatibility
  - `LOG_INFO()` - Logging macro (currently no-op)
  - `CHECK_DIF_OK()` - Error checking macro
  - `CHECK_STATUS_OK()` - Status checking macro
  - `test_main()` wrapper

### HMAC DIF Implementation

- **`dif_hmac_sep.h`** - HMAC DIF header
  - OpenTitan-compatible HMAC API
  - Register offset definitions
  - Data structure definitions

- **`dif_hmac_sep.c`** - HMAC DIF implementation
  - Direct register access for SEP platform
  - SHA-256 and HMAC modes
  - FIFO operations
  - Digest reading

- **`hmac_testutils.h`** - HMAC test utilities
  - Helper functions for test code
  - Polling utilities
  - Digest comparison

## Usage

### Porting an OpenTitan Test

1. **Create a new test directory** in `sw/sep-vp-tests/`
   ```bash
   mkdir opentitan-<testname>-test
   cd opentitan-<testname>-test
   ```

2. **Copy the test source** from OpenTitan and adapt includes:
   ```c
   // Replace OpenTitan includes:
   // #include "sw/device/lib/dif/dif_hmac.h"
   // #include "sw/device/lib/testing/test_framework/ottf_main.h"
   
   // With compatibility layer includes:
   #include "dif_hmac_sep.h"
   #include "ottf_compat.h"
   #include "base_compat.h"
   ```

3. **Create Makefile**:
   ```makefile
   EXTRA_CFLAGS = -I../opentitan-compat
   SRCS = start.S main.c ../opentitan-compat/dif_hmac_sep.c
   OBJS = $(SRCS:.c=.o)
   OBJS := $(OBJS:.S=.o)
   TARGET = opentitan-<testname>-test
   include ../Makefile.common
   ```

4. **Copy standard files** from an existing test:
   - `start.S` - Startup code
   - `link.ld` - Linker script

5. **Build and run**:
   ```bash
   make
   make sim
   ```

## Limitations

This is a **minimal** compatibility layer with the following limitations:

1. **Limited DIF Coverage**: Only HMAC DIF is currently implemented
2. **No Logging**: `LOG_INFO()` is currently a no-op (can be implemented with UART)
3. **Simplified Error Handling**: Errors halt execution instead of proper error reporting
4. **Single HMAC Instance**: Only supports one HMAC peripheral
5. **No Interrupt Support**: All operations use polling

## Extending the Compatibility Layer

To add support for additional OpenTitan peripherals:

1. Create `dif_<peripheral>_sep.h` and `dif_<peripheral>_sep.c`
2. Implement the DIF functions using direct register access
3. Add test utilities in `<peripheral>_testutils.h` if needed
4. Update this README with the new peripheral support

## Memory Map Compatibility

| Component | OpenTitan (Earlgrey) | SEP Platform | Compatible? |
|-----------|---------------------|--------------|-------------|
| HMAC      | 0x40026000          | 0x40026000   | ✅ Yes      |
| ROM       | Varies              | 0x10000000   | ⚠️ Different |
| SRAM      | Varies              | 0x10100000   | ⚠️ Different |

**Note**: Tests are recompiled for the SEP platform memory map, so ROM/SRAM differences are handled at build time.

## Example: HMAC Smoketest

See `../opentitan-hmac-test/` for a complete example of a ported OpenTitan test.

This test:
- Uses the same test vectors as OpenTitan
- Runs SHA-256 and HMAC operations
- Validates digest outputs
- Demonstrates the compatibility layer usage

## References

- [OpenTitan Documentation](https://opentitan.org/)
- [OpenTitan HMAC IP](https://opentitan.org/book/hw/ip/hmac/)
- [OpenTitan DIF Guide](https://opentitan.org/book/sw/device/lib/dif/)
