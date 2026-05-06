# GPIO Single-Pin SystemC Model

## Overview

This directory contains a **SystemC transaction-level model (TLM)** of the OpenTitan GPIO IP. The model implements a single GPIO pin with comprehensive functionality including:

- **Register-based control** via TLM interface (APB4/AXI4-Lite compatible)
- **LSIO interface support** for alternative control path
- **Interrupt generation** (edge-triggered and level-sensitive)
- **PAD configuration** (drive strength, pull resistors, Schmitt trigger)
- **Hardware strap sampling** at reset
- **Security access filtering** with protection level enforcement

For multi-pin GPIO configurations, instantiate multiple instances with different base addresses.

---

## Directory Structure

```
models/ot/gpio/
├── model/
│   ├── src/           # GPIO model implementation
│   └── inc/           # Header files (gpio.h, gpio_base.h, etc.)
├── test/
│   ├── src/           # Testbench implementation
│   └── inc/           # Test headers
├── gpio-knowledge-base/
│   └── *.rdl          # SystemRDL register definitions
├── Makefile           # Build system
├── makefile.config    # SystemC paths configuration
└── doxyfile_gpio      # Doxygen configuration
```

---

## Prerequisites

Ensure the following dependencies are installed and correctly configured:

| Dependency | Version | Notes |
|------------|---------|-------|
| **SystemC** | 3.0.1 or later | Build and install from [Accellera SystemC](https://www.accellera.org/downloads/standards/systemc) |
| **g++/gcc** | 7.0+ | C++14 compatible compiler |
| **make** | 4.0+ | GNU Make |
| **lcov** | 1.14+ | Required for coverage reports (optional) |
| **gcov** | - | Bundled with GCC (optional) |
| **cppcheck** | 2.0+ | For static analysis (optional) |
| **doxygen** | 1.8.13+ | For documentation generation (optional) |

---

## Configuration

### SystemC and CCI Setup

Set environment variables before building:

```bash
export SYSTEMC_HOME=/usr/local/systemc301
export CCI_HOME=/path/to/cci  # Optional, if CCI is in a custom location
```

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

The GPIO model supports multiple build configurations optimized for different purposes:

| Build Type | Purpose | Compiler Flags | Logging Level | Output |
|------------|---------|---------------|---------------|--------|
| **Debug** | Development, full logging | `-g -O0` | VERBOSE (3) | `build/debug/gpio_test` |
| **Release** | Production, optimized | `-O3 -DNDEBUG` | ERRORS ONLY (0) | `build/release/gpio_test` |
| **ASAN** | Memory error detection | `-fsanitize=address` | INFO (2) | `build/asan/gpio_test` |
| **Coverage** | Code coverage analysis | `--coverage` | WARN (1) | `build/coverage/gpio_test` |

### Logging Levels
- **0**: Errors only
- **1**: Warnings and errors
- **2**: Info, warnings, errors
- **3**: Full debug/trace output

---

## Building the Model

The GPIO model uses **CMake** as its build system.

### Prerequisites

Set the `SYSTEMC_HOME` environment variable:

```bash
export SYSTEMC_HOME=/usr/local/systemc301
```

Optionally set `CCI_HOME` if CCI is installed in a custom location:

```bash
export CCI_HOME=/path/to/cci
```

### Build Types

**Debug Build** - Full logging, debug symbols, no optimization:

```bash
cd models/ot/gpio
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
./debug/gpio_test
```

**Release Build** - Optimized, errors-only logging:

```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
make
./release/gpio_test
```

**ASAN Build** - Memory error detection with AddressSanitizer:

```bash
cmake -DCMAKE_BUILD_TYPE=ASAN ..
make
./asan/gpio_test
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
make gpio_cppcheck
cat cppcheck_report.txt
```

### Build Features

- **CSML Verbosity Control**: Automatically configured per build type
  - Debug: Level 3 (full trace)
  - Release: Level 0 (errors only)
  - ASAN: Level 2 (info)
  - Coverage: Level 1 (warnings)
- **CCI Library Detection**: Searches `$CCI_HOME` → `$SYSTEMC_HOME` → system paths
- **Smart Test Building**:
  - Standalone builds: Tests built by default
  - VP integration: Only library built (BUILD_TESTS=OFF automatically)
  - Override with `-DBUILD_TESTS=ON` or `-DBUILD_TESTS=OFF` if needed

### Cleaning Build

```bash
cd build
make clean           # Remove build artifacts
cd .. && rm -rf build  # Full clean
```

---

## Running the Model

Build and run the GPIO test:

```bash
cd models/ot/gpio
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
./debug/gpio_test
```

For other build types:

```bash
./release/gpio_test    # Release build
./asan/gpio_test       # ASAN build
```

---

## Verification & Testing

The testbench (`test/src/testbench.cpp`) includes comprehensive tests covering:

### Register Access Tests
- ✅ Read/write functionality for all accessible registers
- ✅ Read-only register protection
- ✅ Write-only register behavior
- ✅ Reset functionality validation
- ✅ Reserved bit handling

### GPIO I/O Tests
- ✅ Comprehensive GPIO input/output modes
- ✅ Mode transitions (input ↔ output)
- ✅ Output toggling and rapid input changes
- ✅ Port binding verification

### Interrupt Tests
- ✅ Rising/falling edge interrupts
- ✅ Level-high/level-low interrupts
- ✅ Interrupt enable/disable
- ✅ Interrupt type switching
- ✅ Rapid edge handling
- ✅ Stress testing with rapid interrupts

### LSIO Interface Tests
- ✅ LSIO interface control
- ✅ LSIO output control
- ✅ Priority between register and LSIO control
- ✅ LSIO disable functionality

### PAD Configuration Tests
- ✅ Drive strength configuration (all levels)
- ✅ Pull-up/pull-down resistor configuration
- ✅ Schmitt trigger enable/disable
- ✅ PAD config enable/disable

### Hardware Strap Tests
- ✅ Strap sampling at reset
- ✅ Strap value capture (0 and 1)
- ✅ Non-strap pin behavior
- ✅ Multiple reset cycles

### Security & Access Filter Tests
- ✅ Basic access filtering
- ✅ Write protection enforcement
- ✅ Read protection enforcement
- ✅ SEP vs non-SEP access levels
- ✅ Mixed protection values

### Integration & Stress Tests
- ✅ State transitions (full cycle)
- ✅ Control path switching
- ✅ Back-to-back register writes
- ✅ Boundary condition testing
- ✅ Full sequence integration tests

### Test Execution

Tests run automatically when you execute the binary:

```bash
./Obj/debug/sim.x
```

**Expected Output**: Console logs showing test progress and PASS/FAIL status for each test.

---

## Code Coverage

Generate code coverage reports (model folder only):

```bash
cd models/ot/gpio
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Coverage ..
make
make coverage
```

This will:
1. Build the model with coverage instrumentation
2. Run the testbench automatically
3. Generate coverage data using `gcov`
4. Create an HTML report with `lcov`

**View Coverage Report**:

```bash
# Report location
firefox coverage/html/index.html
# or
google-chrome coverage/html/index.html
```

---

## Static Analysis

Run `cppcheck` static analysis (model folder only):

```bash
cd models/ot/gpio
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make gpio_cppcheck
```

**Analysis Report**:

```bash
cat cppcheck_report.txt
```

**Analysis Configuration**:
- Enables all checks
- Suppresses system include warnings
- Generates detailed reports with line numbers
- Uses GCC-style output format

---

## Documentation

Generate Doxygen documentation:

```bash
cd models/ot/gpio
mkdir build && cd build
cmake ..
make docs
```

**View Documentation**:

```bash
firefox docs/html/index.html
```

---

## Cleaning

Remove all build artifacts:

```bash
cd build
make clean
```

Or completely remove the build directory:

```bash
cd models/ot/gpio
rm -rf build
```

This removes:
- Object files
- Executables
- Coverage files (`*.gcov`, `*.info`)
- CMake cache and generated files

---

## Build Examples

### Quick Development Cycle

```bash
cd models/ot/gpio
mkdir build && cd build

# Build debug version
cmake -DCMAKE_BUILD_TYPE=Debug ..
make

# Run tests
./debug/gpio_test
```

### Release Build for Performance Testing

```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
make
./release/gpio_test
```

### Memory Error Checking

```bash
cmake -DCMAKE_BUILD_TYPE=ASAN ..
make
./debug/gpio_test
```

### Generate Coverage Report

```bash
cmake -DCMAKE_BUILD_TYPE=Coverage ..
make
make coverage
# View report in browser
firefox coverage/html/index.html
```

### Run Static Analysis

```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
make gpio_cppcheck
cat cppcheck_report.txt
```

---

## Troubleshooting

### Issue: CMake cannot find SystemC

**Error**: `SYSTEMC_HOME is not set and SystemC::systemc not found`

**Solution**: Set the SYSTEMC_HOME environment variable:

```bash
export SYSTEMC_HOME=/usr/local/systemc301
```

### Issue: Cannot find systemc library

**Error**: `Could not find systemc library under $SYSTEMC_HOME`

**Solution**: Verify SystemC is installed and `lib-linux64` directory exists:

```bash
ls $SYSTEMC_HOME/lib-linux64/libsystemc.so
# or
ls $SYSTEMC_HOME/lib/libsystemc.so
```

### Issue: CSML headers not found

**Solution**: Verify CSML exists at `../../utils/csml/`:

```bash
ls ../../utils/csml/inc/
```

### Issue: CCI libraries not found

**Warning**: `CCI libraries not found. Building without CCI support.`

**Solution**: This is optional. If you need CCI, either:
1. Set `CCI_HOME` environment variable
2. Install CCI libraries to `$SYSTEMC_HOME/lib`

```bash
export CCI_HOME=/path/to/cci
```

### Issue: Coverage report not generated

**Solution**: Ensure `lcov` and `gcov` are installed:

```bash
sudo apt-get install lcov  # Ubuntu/Debian
brew install lcov          # macOS
```

---

## Register Map Reference

The GPIO model implements the following registers (per pin):

| Offset | Register | Type | Description |
|--------|----------|------|-------------|
| `0x00` | `DATA_CTRL` | RW | Data output, direction control, interface select |
| `0x08` | `ACCESS_FILTER` | RW | Security filtering, protection level requirements |
| `0x10` | `CONTROL` | RW | PAD configuration, strap sampling, interrupts |

For detailed register field definitions, see:
- `gpio-knowledge-base/gpio_intf.rdl`
- `gpio-knowledge-base/gpio_ctrl.rdl`
- Generated Doxygen documentation

---

## Related Documentation

- **OpenTitan GPIO Specification**: [opentitan.org](https://opentitan.org)
- **SystemC Standard**: [Accellera SystemC](https://www.accellera.org/downloads/standards/systemc)
- **CSML Library**: `../../utils/csml/README.md`
- **RTL Knowledge Base**: `gpio-knowledge-base/README.adoc`

---

## Support

For issues or questions:

1. Check the **Troubleshooting** section above
2. Review generated documentation: `make docs`
3. Examine test logs in `Debug_Log.txt`
4. Contact the model maintainers

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2025 | Initial RDL-based single-pin implementation |

---

**Model developed by Vayavya Labs**
