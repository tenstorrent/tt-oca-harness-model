# OTBN SystemC Model

## Overview

This directory contains a **SystemC transaction-level model (TLM)** of the OpenTitan Big Number Accelerator (OTBN) IP. The model implements a cryptographic coprocessor for public-key cryptography operations with comprehensive functionality including:

- **RSA-2048 support** - Modular exponentiation using OpenSSL BIGNUM
- **Pluggable algorithm architecture** - Easy extension for ECDSA, X25519, etc.
- **Register-based control** via TLM target socket
- **Memory windows** - 8KB IMEM (instruction) + 3KB DMEM (data)
- **Wide Data Registers (WDR)** - 32x256-bit registers for cryptographic operations
- **State machine** - IDLE, BUSY (EXECUTE/SECWIPE), LOCKED states
- **External interfaces** - EDN, Key Manager, OTP, Life Cycle Controller
- **Interrupt generation** - Done interrupt (intr_done)
- **Security features** - Secure wipe, error handling, locked state
- **OpenSSL integration** - RSA cryptographic operations delegated to OpenSSL library

This model is suitable for both standalone testing and integration into larger SoC virtual platforms.

---

## Directory Structure

```
models/ot/otbn/
├── model/
│   ├── src/           # OTBN model implementation
│   │   ├── otbn.cpp            # Main model implementation
│   │   └── otbn_base.cpp       # Auto-generated register base
│   └── inc/           # Header files
│       ├── otbn.h              # Main OTBN class with algorithm selection
│       ├── otbn_base.h         # Register base class
│       ├── otbn_register.h     # Register definitions
│       └── otbn_interfaces.h   # Interface classes
├── test/
│   ├── src/           # Testbench and test functions
│   │   ├── testbench.cpp       # Main test orchestration (~40 tests)
│   │   ├── otbn_test.cpp       # TLM initiator implementation
│   │   └── otbn_basetest.cpp   # Test infrastructure
│   └── inc/           # Test headers
├── otbn-knowledge-base/
│   ├── registers.md            # Register specifications
│   ├── interfaces.md           # Interface descriptions
│   └── theory_of_operation.md  # Functional behavior
├── otbn-generated-docs/
│   └── *.md           # Generated design documentation
├── CMakeLists.txt     # CMake build system
├── Doxyfile.in        # Doxygen template
├── Makefile.legacy    # Legacy Makefile (deprecated)
└── CLAUDE.md          # Design documentation context
```

---

## Prerequisites

Ensure the following dependencies are installed and correctly configured:

| Dependency | Version | Notes |
|------------|---------|-------|
| **SystemC** | 3.0.1 or later | Build and install from [Accellera SystemC](https://www.accellera.org/downloads/standards/systemc) |
| **OpenSSL** | 3.3.1 or later | Required for RSA-2048 algorithm |
| **CMake** | 3.14+ | Build system |
| **g++/gcc** | 7.0+ | C++17 compatible compiler |
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

The OTBN model supports multiple build configurations optimized for different purposes:

| Build Type | Purpose | Compiler Flags | Logging Level | Output |
|------------|---------|---------------|---------------|--------|
| **Debug** | Development with full logging | `-g -O0` | Full (level 3) | `build/debug/otbn_test` |
| **Release** | Optimized production build | `-O3 -DNDEBUG -Werror` | Errors only (level 0) | `build/release/otbn_test` |
| **ASAN** | Memory error detection | `-fsanitize=address` | Info (level 2) | `build/asan/otbn_test` |
| **Coverage** | Code coverage analysis | `--coverage` | Warnings (level 1) | `build/coverage/otbn_test` |

---

## Building the Model

The OTBN model uses **CMake** as its build system.

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
cd models/ot/otbn
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
./debug/otbn_test
```

**Release Build** - Optimized, errors-only logging:

```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
make
./release/otbn_test
```

**ASAN Build** - Memory error detection with AddressSanitizer:

```bash
cmake -DCMAKE_BUILD_TYPE=ASAN ..
make
./asan/otbn_test
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
make otbn_cppcheck
cat cppcheck_report.txt
```

**Documentation** - Generate Doxygen documentation:

```bash
make otbn_docs
firefox docs/html/index.html
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
- **Template Depth**: Automatically increased to 4096 for OTBN's complex algorithm templates

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
cd models/ot/otbn
mkdir build && cd build

# Build in debug mode
cmake -DCMAKE_BUILD_TYPE=Debug ..
make

# Run the testbench (all tests enabled by default)
./debug/otbn_test
```

### Test Output

The testbench executes **26 comprehensive tests** covering:
- Register reset values and access controls
- Read/write access patterns
- IMEM/DMEM memory access
- Command execution (EXECUTE, SECWIPE_DMEM, SECWIPE_IMEM)
- State machine transitions
- Algorithm invocation (RSA-2048)
- Interface interactions (EDN, Key Manager, OTP, Life Cycle)
- Interrupt generation
- Error handling and locked state
- Memory protection
- Load checksum validation

Each test reports PASS/FAIL status with detailed logging.

---

## Algorithm Support

The OTBN model uses a pluggable algorithm architecture:

| Algorithm | Status | Selection String | Description |
|-----------|--------|-----------------|-------------|
| **RSA-2048** | ✅ Implemented | `"rsa_2048"`, `"RSA-2048"`, `"RSA_2048"` | RSA-2048 modular exponentiation using OpenSSL BIGNUM |
| **Summation** | ✅ Implemented | `"summation"`, `"SUMMATION"`, `"sum"` | Reference algorithm (byte summation) |
| ECDSA-P256 | ⚠️ Placeholder | `"ecdsa_p256"` | Not yet implemented |
| X25519 | ⚠️ Placeholder | `"x25519"` | Not yet implemented |

### Algorithm Selection

Algorithms are selected in the testbench (`test/src/testbench.cpp`):

```cpp
// Select RSA-2048 (default)
dut = new otbn_ip("otbn_dut", 0x10000, "rsa_2048");

// Or select Summation algorithm
dut = new otbn_ip("otbn_dut", 0x10000, "summation");
```

### DMEM Layout Conventions

#### RSA-2048 DMEM Layout:
```
0x000-0x0FF (256 bytes): base/message (input)
0x100-0x1FF (256 bytes): exponent (input)
0x200-0x2FF (256 bytes): modulus (input)
0x300-0x3FF (256 bytes): result (output)
```

#### Summation DMEM Layout:
```
DMEM[0]: N (number of input bytes)
DMEM[1..N]: Input bytes to sum
DMEM[N+1]: Result length (written by algorithm)
DMEM[N+2..]: Result (sum as multi-byte value)
```

---

## Test Coverage

The OTBN model includes **26 functional tests** organized by category:

### Basic Infrastructure Tests (5 tests)
| Test | Description |
|------|-------------|
| **test_reset_values** | Verify register reset values |
| **test_register_read_write** | Basic register access |
| **test_readonly_registers** | Read-only register enforcement |
| **test_memory_access** | IMEM/DMEM basic access |
| **test_port_binding** | Interface port verification |

### Core Functional Tests (15 tests from test plan)
| Test | Description |
|------|-------------|
| **test_execute_command** | EXECUTE command triggers algorithm |
| **test_secwipe_dmem** | Secure wipe data memory |
| **test_secwipe_imem** | Secure wipe instruction memory |
| **test_state_transitions** | State machine (IDLE → BUSY → IDLE) |
| **test_algorithm_invocation** | Algorithm execution flow |
| **test_interrupt_generation** | Done interrupt (intr_done) |
| **test_error_handling** | ERR_BITS and FATAL_ALERT_CAUSE |
| **test_locked_state** | Locked state behavior |
| **test_edn_interface** | Entropy Distribution Network |
| **test_keymgr_interface** | Key Manager sideload keys |
| **test_otp_interface** | OTP scrambling keys |
| **test_lc_interface** | Life Cycle escalation/RMA |
| **test_memory_protection** | Protected DMEM region |
| **test_load_checksum** | CRC-32-IEEE checksum |
| **test_insn_cnt** | Instruction count tracking |

### Compliance Validation Tests (9 tests)
| Test | Description |
|------|-------------|
| **test_fatal_alert_mapping** | FATAL_ALERT_CAUSE bit mapping |
| **test_ctrl_access** | CTRL register access control |
| **test_internal_secwipe** | Internal secure wipe behavior |
| **test_cmd_silent_ignore** | CMD write during BUSY |
| **test_insn_cnt_callback** | INSN_CNT write callback |
| **test_err_bits_locked** | ERR_BITS in locked state |
| **test_lc_escalation** | Life cycle escalation |
| **test_lc_rma** | Life cycle RMA request |
| **test_unrecognized_command** | Invalid command handling |

### Comprehensive Integration Tests (13 tests)
| Test | Description |
|------|-------------|
| **test_state_machine_comprehensive** | Full state machine coverage |
| **test_memory_comprehensive** | Memory access patterns |
| **test_algorithm_comprehensive** | End-to-end algorithm execution |
| Additional state/memory/algorithm tests | Various edge cases and scenarios |

### Enabling/Disabling Tests

Tests are controlled in `test/src/testbench.cpp`. By default, all tests are enabled and run sequentially.

---

## Code Coverage

Generate code coverage reports with:

```bash
cd models/ot/otbn
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
cd models/ot/otbn
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make otbn_cppcheck
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
cd models/ot/otbn
mkdir build && cd build
cmake ..
make otbn_docs
```

**View Documentation**:

```bash
firefox docs/html/index.html
```

---

## Verification Coverage

### ✅ Fully Verified Features

- **Registers**: Full register map with access controls ✅
- **Memory**: 8KB IMEM + 3KB DMEM windows ✅
- **State Machine**: IDLE, BUSY, LOCKED transitions ✅
- **Commands**: EXECUTE, SECWIPE_DMEM, SECWIPE_IMEM ✅
- **Algorithms**: RSA-2048, Summation ✅
- **Interrupts**: Done interrupt tested ✅
- **Alerts**: Fatal and recoverable alerts ✅
- **Error Handling**: ERR_BITS, FATAL_ALERT_CAUSE ✅
- **Interfaces**: EDN, Key Manager, OTP, Life Cycle ✅
- **Security**: Secure wipe, locked state, memory protection ✅
- **Checksum**: CRC-32-IEEE validation ✅
- **WDR Registers**: 32x256-bit Wide Data Registers ✅

### Test Execution Summary

```
Total Tests: 26
Typical Runtime: ~3-5 seconds (debug mode)
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

**Solution**: Ensure CSML library exists at `../../utils/csml/`. The path is relative to the OTBN directory.

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

The OTBN model has configurable parameters:

### Memory Configuration
- **IMEM_SIZE** = 8KB (instruction memory)
- **DMEM_SIZE** = 3KB (data memory)
- **Memory size** configurable via constructor (default: 0x10000 = 64KB address space)

### Register Configuration
- **NUM_WDR_REGISTERS** = 32 (Wide Data Registers, 256-bit each)
- WDRs used for cryptographic operations (accessible via callbacks)

### Algorithm Configuration
- Algorithm selection via constructor string parameter
- Pluggable architecture for easy extension

---

## Integration Notes

### TLM Socket Interface

```cpp
// Main register/memory access
tlm_utils::simple_target_socket<otbn_ip> tl_socket;

// Key Manager interface (TLM)
tlm_utils::simple_target_socket<otbn_ip> keymgr_tl_socket;

// Interrupts
sc_out<bool> intr_done;  // Algorithm completion

// Alerts
sc_out<bool> alert_fatal;      // Fatal alert
sc_out<bool> alert_recov;      // Recoverable alert
```

### Address Map

| Range | Description |
|-------|-------------|
| `0x0000 - 0x3FFF` | Control/status registers |
| `0x4000 - 0x7FFF` | IMEM window (8KB, 2048 words) |
| `0x8000 - 0xBFFF` | DMEM window (3KB, 768 words) |

### Key Manager TLM Interface

| Address | Register | Description |
|---------|----------|-------------|
| `0x00` | KEY_S0_L | Key Share 0 Lower (maps to WDR20) |
| `0x20` | KEY_S0_H | Key Share 0 Upper (maps to WDR21) |
| `0x40` | KEY_S1_L | Key Share 1 Lower (maps to WDR22) |
| `0x60` | KEY_S1_H | Key Share 1 Upper (maps to WDR23) |

---

## Adding Custom Algorithms

To add a new algorithm:

1. **Create algorithm class** in `model/inc/otbn.h` inheriting from `otbn_algorithm`
2. **Implement required methods**: `execute()`, `get_instruction_count()`, `reset()`, `message_objects()`
3. **Add to selection logic** in `otbn.cpp` `select_algorithm()` method
4. **Define DMEM layout** convention for inputs/outputs
5. **Add test case** in testbench

See `IMPLEMENTATION.md` for detailed algorithm development guide.

---

## Additional Resources

- **Design Documentation**: `otbn-generated-docs/` directory
- **Knowledge Base**: `otbn-knowledge-base/` directory
- **Implementation Guide**: `IMPLEMENTATION.md` for detailed build and algorithm guide
- **Architecture Guide**: `CLAUDE.md` for complete architecture and design patterns
- **CSML Library**: `../../utils/csml/` for register abstraction details

---

## Notes

- All 26 tests are verified and passing
- OpenSSL library handles RSA-2048 cryptographic computations
- Model focuses on software-visible behavior and transaction-level operations
- Test vectors validated against OpenSSL reference implementation
- Coverage analysis targets model code only (test code excluded)
- **Build System**: Migrated from Makefile to CMake for better cross-platform support and VP integration

---

## Quick Start

```bash
# 1. Set environment variables
export SYSTEMC_HOME=/usr/local/systemc301

# 2. Build and run
cd models/ot/otbn
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
./debug/otbn_test

# 3. Check results
# Look for test PASS/FAIL messages
# Expected: All 26 tests passing
```
