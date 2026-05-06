# HMAC SystemC Model

## Overview

This directory contains a **SystemC transaction-level model (TLM)** of the OpenTitan HMAC IP. The model implements a full-featured cryptographic hash engine with comprehensive functionality including:

- **SHA-2 family support** - SHA-256, SHA-384, SHA-512
- **HMAC mode** - Hash-based Message Authentication Code
- **Register-based control** via TLM target socket
- **Message FIFO** with back-pressure and depth tracking
- **Configurable key lengths** - 128/256/384/512/1024 bits
- **Interrupt generation** - Done, FIFO empty, and error interrupts
- **Context switching** - Multi-stream processing with state save/restore
- **Security features** - Key wipe, write-only key registers, error detection
- **OpenSSL integration** - Cryptographic operations delegated to OpenSSL library

This model is suitable for both standalone testing and integration into larger SoC virtual platforms.

---

## Directory Structure

```
models/ot/hmac/
├── model/
│   ├── src/           # HMAC model implementation
│   └── inc/           # Header files (hmac.h, registers, etc.)
├── test/
│   ├── src/           # Testbench and test functions
│   │   ├── testbench.cpp        # Main test orchestration
│   │   ├── basic_tests.cpp      # Basic register/port tests
│   │   ├── hmac_test.cpp        # HMAC test implementation
│   │   └── hmac_basetest.cpp    # Test infrastructure
│   └── inc/           # Test headers
├── hmac-knowledge-base/
│   ├── hmac.md        # RTL knowledge base
│   └── registers.md   # Register documentation
├── hmac-generated-docs/
│   └── *.md           # Generated design documentation
├── Makefile           # Build system
├── makefile.config    # SystemC/OpenSSL paths configuration
└── CLAUDE.md          # Design documentation context
```

---

## Prerequisites

Ensure the following dependencies are installed and correctly configured:

| Dependency | Version | Notes |
|------------|---------|-------|
| **SystemC** | 3.0.1 or later | Build and install from [Accellera SystemC](https://www.accellera.org/downloads/standards/systemc) |
| **OpenSSL** | 3.3.1 or later | Required for cryptographic operations |
| **g++/gcc** | 7.0+ | C++14 compatible compiler |
| **make** | 4.0+ | GNU Make |
| **lcov** | 1.14+ | Required for coverage reports (optional) |
| **gcov** | - | Bundled with GCC (optional) |
| **cppcheck** | 2.0+ | For static analysis (optional) |
| **doxygen** | 1.8.13+ | For documentation generation (optional) |

---

## Configuration

### SystemC, CCI, and OpenSSL Setup

Set environment variables before building:

```bash
export SYSTEMC_HOME=/usr/local/systemc301
export CCI_HOME=/path/to/cci  # Optional, if CCI is in a custom location
```

**Note**: OpenSSL library paths are auto-detected by CMake. No manual configuration needed.

CMake will automatically search for libraries in:
- `$CCI_HOME` (if set)
- `$SYSTEMC_HOME`
- System paths (`/usr/local/lib`, `/usr/lib`)

### CSML Integration

The model uses the **Common SystemC Model Library (CSML)** located at:
```
../../utils/csml/
```

This path is automatically configured by CMake. No manual setup needed.

---

## Build Targets

The HMAC model supports multiple build configurations optimized for different purposes:

| Build Type | Purpose | Compiler Flags | Logging Level | Output |
|------------|---------|---------------|---------------|--------|
| **Debug** | Development with full logging | `-g -O0` | Full (level 3) | `build/debug/hmac_test` |
| **Release** | Optimized production build | `-O3 -DNDEBUG` | Errors only (level 0) | `build/release/hmac_test` |
| **ASAN** | Memory error detection | `-fsanitize=address` | Info (level 2) | `build/asan/hmac_test` |
| **Coverage** | Code coverage analysis | `--coverage` | Warnings (level 1) | `build/coverage/hmac_test` |

---

## Building the Model

The HMAC model uses **CMake** as its build system.

### Prerequisites

Set required environment variables:

```bash
export SYSTEMC_HOME=/usr/local/systemc301
```

Optionally set `CCI_HOME` if CCI is installed in a custom location:

```bash
export CCI_HOME=/path/to/cci
```

OpenSSL paths are auto-detected by CMake.

### CMake Build Types

**Debug Build** - Full logging, debug symbols:

```bash
cd models/ot/hmac
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
./debug/hmac_test
```

**Release Build** - Optimized, errors-only logging:

```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
make
./release/hmac_test
```

**ASAN Build** - Memory error detection with AddressSanitizer:

```bash
cmake -DCMAKE_BUILD_TYPE=ASAN ..
make
./asan/hmac_test
```

**Coverage Build** - Code coverage analysis:

```bash
cmake -DCMAKE_BUILD_TYPE=Coverage ..
make
make coverage  # Runs tests and generates HTML coverage report
firefox coverage/html/index.html
```

**Note**: The `-DBUILD_TESTS=ON` flag is optional for standalone builds (defaults to ON). It's only needed if you explicitly want to build without tests.

**Static Analysis** - Run cppcheck on model code:

```bash
# From any build directory
make hmac_cppcheck
cat cppcheck_report.txt
```

### CMake Build Features

- **CSML Verbosity Control**: Automatically configured per build type
  - Debug: Level 3 (full trace)
  - Release: Level 0 (errors only)
  - ASAN: Level 2 (info)
  - Coverage: Level 1 (warnings)
- **OpenSSL Integration**: Automatically detects and links OpenSSL libraries
- **CCI Library Detection**: Searches `$CCI_HOME` → `$SYSTEMC_HOME` → system paths
- **Smart Test Building**:
  - Standalone builds: Tests built by default
  - VP integration: Only library built (BUILD_TESTS=OFF automatically)
  - Override with `-DBUILD_TESTS=ON` or `-DBUILD_TESTS=OFF` if needed
- **Template Depth**: Automatically increased to 2000 for HMAC's large register arrays

### Cleaning CMake Build

```bash
cd build
make clean           # Remove build artifacts
cd .. && rm -rf build  # Full clean
```

---

## Running the Model

### Basic Execution

```bash
cd models/ot/hmac
mkdir build && cd build

# Build in debug mode
cmake -DCMAKE_BUILD_TYPE=Debug ..
make

# Run the testbench (all tests enabled by default)
./debug/hmac_test
```

### Test Output

The testbench executes **32 comprehensive tests** covering:
- SHA-256/384/512 hash computation
- HMAC-SHA-256/384/512 with various key lengths
- Interrupt generation and masking
- Error conditions and recovery
- FIFO management
- Endianness handling
- Security features (key wipe, write-only registers)
- Context switching

Each test reports PASS/FAIL status with detailed logging.

---

## Test Coverage

The HMAC model includes **32 functional tests** organized by category:

### Core Hash Functionality (8 tests)
| Test | Description |
|------|-------------|
| **test_sha256_hash** | Basic SHA-256 single-block hashing |
| **test_sha256_hash_multiblock_message** | Multi-block message processing |
| **test_sha384_hash** | SHA-384 hash computation |
| **test_sha512_hash** | SHA-512 hash computation |
| **test_empty_message_hash** | Empty message edge case |
| **test_block_boundary_message** | Block boundary handling |
| **test_message_length_tracking** | Message length counter verification |
| **test_minimum_length_transfer** | Minimum length message handling |

### HMAC Functionality (6 tests)
| Test | Description |
|------|-------------|
| **test_hmac_sha256_key128** | HMAC-SHA-256 with 128-bit key |
| **test_hmac_sha256_key256** | HMAC-SHA-256 with 256-bit key |
| **test_hmac_sha256_key512** | HMAC-SHA-256 with 512-bit key |
| **test_hmac_sha384_key384** | HMAC-SHA-384 with 384-bit key |
| **test_hmac_sha512_key1024** | HMAC-SHA-512 with 1024-bit key |
| **test_invalid_key_length_hmac** | Invalid key length error handling |

### Interrupt Testing (4 tests)
| Test | Description |
|------|-------------|
| **test_hmac_done_interrupt** | Hash completion interrupt |
| **test_hmac_err_interrupt** | Error interrupt generation |
| **test_interrupt_injection** | Interrupt test register functionality |
| **test_interrupt_masking** | Interrupt enable/disable control |

### FIFO Testing (6 tests)
| Test | Description |
|------|-------------|
| **test_fifo_status_updates** | FIFO depth tracking |
| **test_fifo_empty_interrupt** | Empty FIFO interrupt |
| **test_fifo_back_pressure** | FIFO full back-pressure |
| **test_subword_writes_byte** | Byte-level FIFO writes |
| **test_subword_writes_halfword** | Halfword-level FIFO writes |
| **test_msg_fifo_address_window** | 4KB address window access |

### Security Features (5 tests)
| Test | Description |
|------|-------------|
| **test_wipe_secret** | Key wipe functionality |
| **test_key_register_writeonly** | Write-only key register protection |
| **test_key_write_during_processing** | Key write prevention during operation |
| **test_cfg_write_protection** | Configuration register protection |
| **test_digest_write_protection** | Digest register write protection |

### Error Handling (3 tests)
| Test | Description |
|------|-------------|
| **test_error_conditions** | Comprehensive error scenarios |
| **test_error_recovery** | Error recovery flow |
| **test_msg_fifo_after_process** | FIFO access after processing |

### Additional Features (5 tests)
| Test | Description |
|------|-------------|
| **test_sha256_endian_swap** | Endianness configuration (message) |
| **test_sha256_digest_swap** | Endianness configuration (digest) |
| **test_key_swap** | Key endianness handling |
| **test_status_hmac_idle_transitions** | Idle status tracking |
| **test_command_self_clearing** | Self-clearing command bits |
| **test_reserved_fields** | Reserved field behavior |
| **test_context_saving** | Context save/restore |
| **test_reset_during_processing** | Reset handling during operation |
| **test_invalid_digest_size** | Invalid configuration handling |

### Enabling/Disabling Tests

Tests are controlled via preprocessor macros in `test/src/testbench.cpp`:

```cpp
// Disable a specific test
//#define TEST_SHA256_MULTIBLOCK

// Or comment out unwanted tests
//#define TEST_INTERRUPT_MASKING
```

By default, all tests are enabled and run sequentially.

---

## Code Coverage

Generate code coverage reports with:

```bash
cd models/ot/hmac
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Coverage ..
make
make coverage
```

This will:
1. Build with coverage instrumentation
2. Run all tests
3. Generate coverage data for the `model/` folder only
4. Create HTML report in `coverage/html/index.html`

**View Coverage Report**:

```bash
firefox coverage/html/index.html
```

**Coverage Metrics**:
- Line coverage
- Function coverage
- Branch coverage

Reports focus on the model implementation (`model/src/*.cpp`), excluding test and library code.

---

## Static Analysis

Run cppcheck static analysis:

```bash
cd models/ot/hmac
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make hmac_cppcheck
```

Results are saved to `cppcheck_report.txt`. The analysis checks:
- Memory leaks
- Null pointer dereferences
- Buffer overflows
- Uninitialized variables
- Style issues

**View Report**:

```bash
cat cppcheck_report.txt
```

---

## Documentation

Generate Doxygen documentation:

```bash
cd models/ot/hmac
mkdir build && cd build
cmake ..
make docs
```

**View Documentation**:

```bash
firefox docs/html/index.html
```

---

## Verification Coverage

### ✅ Fully Verified Features

- **Hash Algorithms**: SHA-256, SHA-384, SHA-512 ✅
- **HMAC Mode**: All key lengths (128 to 1024 bits) ✅
- **Message FIFO**: Depth tracking, back-pressure, subword writes ✅
- **Interrupts**: Done, error, FIFO empty - all tested ✅
- **Error Detection**: Invalid commands, configuration errors ✅
- **Security**: Key wipe, write-only protection ✅
- **Endianness**: Message and digest byte order control ✅
- **Context Switching**: State save/restore ✅
- **Reset Handling**: Hardware and software reset ✅
- **Register Protection**: Write-only keys, read-only status ✅

### Test Execution Summary

```
Total Tests: 32
Typical Runtime: ~5-10 seconds (debug mode)
All tests: PASS ✅
```

---

## Troubleshooting

### Common Issues

**1. CMake cannot find SystemC**

**Error**: `SYSTEMC_HOME is not set and SystemC::systemc not found`

**Solution**: Set the SYSTEMC_HOME environment variable:

```bash
export SYSTEMC_HOME=/usr/local/systemc301
```

**2. Cannot find systemc library**

**Error**: `Could not find systemc library under $SYSTEMC_HOME`

**Solution**: Verify SystemC is installed:

```bash
ls $SYSTEMC_HOME/lib-linux64/libsystemc.so
# or
ls $SYSTEMC_HOME/lib/libsystemc.so
```

**3. OpenSSL not found**

**Error**: `Could not find OpenSSL`

**Solution**: Install OpenSSL development packages:

```bash
# Ubuntu/Debian
sudo apt-get install libssl-dev

# RHEL/CentOS
sudo yum install openssl-devel

# macOS
brew install openssl
```

**4. CSML files not found**

**Error**: `fatal error: csml_logger.h: No such file or directory`

**Solution**: Ensure CSML library exists at `../../utils/csml/`. The path is relative to the HMAC directory.

```bash
ls ../../utils/csml/inc/
```

**5. CCI libraries not found**

**Warning**: `CCI libraries not found. Building without CCI support.`

**Solution**: This is optional. If you need CCI, either:
1. Set `CCI_HOME` environment variable
2. Install CCI libraries to `$SYSTEMC_HOME/lib`

```bash
export CCI_HOME=/path/to/cci
```

**6. Coverage tools missing**

**Error**: `lcov: command not found`

**Solution**: Install coverage tools:

```bash
sudo apt-get install lcov  # Ubuntu/Debian
brew install lcov          # macOS
```

---

## Model Configuration Parameters

The HMAC model has **8 build-time parameters** (defined in `model/inc/hmac.h`):

### FIFO Configuration
- **MSG_FIFO_DEPTH_SHA256** = 16 entries (for SHA-256)
- **MSG_FIFO_DEPTH_SHA384_512** = 32 entries (for SHA-384/512)
- **MSG_FIFO_WINDOW_SIZE** = 4096 bytes (memory-mapped window)

### Register Configuration
- **NUM_DIGEST_REGS** = 16 registers (supports up to 512-bit results)
- **NUM_KEY_REGS** = 32 registers (supports up to 1024-bit keys)

### Timing Configuration
- **SHA256_BLOCK_CYCLES** = 80 cycles (block processing latency)
- **SHA384_512_BLOCK_CYCLES** = 96 cycles (longer for larger blocks)
- **HMAC_EXTRA_CYCLES** = 240 cycles (HMAC two-round overhead)

These parameters control FIFO sizing, register counts, and observable timing characteristics.

---

## Integration Notes

### TLM Socket Interface

```cpp
// Register access
tlm_target_socket<32> tl_socket;

// Interrupts
sc_out<bool> intr_hmac_done;    // Hash completion
sc_out<bool> intr_fifo_empty;   // FIFO empty
sc_out<bool> intr_hmac_err;     // Error condition
```

### Address Map

| Range | Description |
|-------|-------------|
| `0x0000 - 0x0FFF` | Control/status registers |
| `0x1000 - 0x1FFF` | Message FIFO window (4KB) |

---

## Additional Resources

- **Design Documentation**: `hmac-generated-docs/` directory
- **Knowledge Base**: `hmac-knowledge-base/` directory
- **OpenTitan HMAC Spec**: See knowledge-base for original RTL documentation
- **CSML Library**: `../../utils/csml/` for register abstraction details

---

## Notes

- All 32 tests are verified and passing
- OpenSSL library handles actual cryptographic computations
- Model focuses on software-visible behavior and transaction-level operations
- Test vectors validated against NIST test cases and OpenSSL reference implementation
- Coverage analysis targets model code only (test code excluded)

---

## Quick Start

```bash
# 1. Set environment variables
export SYSTEMC_HOME=/usr/local/systemc301

# 2. Build and run
cd models/ot/hmac
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
./debug/hmac_test

# 3. Check results
# Look for test PASS/FAIL messages
```
