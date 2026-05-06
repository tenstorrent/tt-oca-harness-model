# SPI Controller SystemC Model

## Overview

This directory contains a **SystemC transaction-level model (TLM)** of the OpenTitan SPI Controller IP (also known as SPI Host). The model implements a full-featured SPI master controller with comprehensive functionality including:

- **Register-based control** via TLM target socket
- **Multi-device support** with configurable chip select lines (1-8 devices)
- **Flexible SPI modes** - Standard, Dual, and Quad SPI
- **Configurable FIFOs** - Separate TX/RX FIFOs with programmable watermarks
- **Command queue** - Multi-segment transaction support
- **Interrupt generation** - Error and event interrupts
- **DMA triggers** - For efficient FIFO management
- **CCI configuration** - JSON-based runtime configuration (optional)

This model is suitable for both standalone testing and integration into larger SoC virtual platforms.

---

## Directory Structure

```
models/ot/spi_controller/
├── model/
│   ├── src/           # SPI Controller model implementation
│   └── inc/           # Header files (spi_controller.h, registers, etc.)
├── test/
│   ├── src/           # Testbench and test functions
│   └── inc/           # Test headers
├── config/
│   ├── spi_controller_default.json      # Default configuration (1 CS, standard FIFOs)
│   ├── spi_controller_multi_device.json # Multi-device configuration (2 CS)
│   └── spi_controller_large_fifo.json   # Large FIFO configuration
├── spi_controller-knowledge-base/
│   ├── spi_controller.md     # RTL knowledge base
│   ├── spi_controller.rdl    # SystemRDL register definitions
│   └── registers.md          # Register documentation
├── spi_controller-generated-docs/
│   └── *.md                  # Generated design documentation
├── CMakeLists.txt            # CMake build configuration
├── Doxyfile.in               # Doxygen template for CMake
└── config/                   # CCI configuration files
```

---

## Prerequisites

Ensure the following dependencies are installed and correctly configured:

| Dependency | Version | Notes |
|------------|---------|-------|
| **SystemC** | 3.0.1 or later | Build and install from [Accellera SystemC](https://www.accellera.org/downloads/standards/systemc) |
| **SystemC CCI** | 1.0.0+ | Configuration, Control and Inspection library (bundled with SystemC 3.0+) |
| **g++/gcc** | 7.0+ | C++17 compatible compiler |
| **cmake** | 3.14+ | CMake build system |
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

### CCI Configuration

SystemC CCI (Configuration, Control and Inspection) is automatically used in all builds. It allows runtime configuration via JSON files in the `config/` directory.

---

## Build Targets

The SPI Controller model supports multiple build configurations optimized for different purposes:

| Build Type | Purpose | Compiler Flags | Logging Level | Output |
|------------|---------|---------------|---------------|--------|
| **Debug** | Development with full logging | `-g -O0` | VERBOSE (3) | `build/debug/spi_controller_test` |
| **Release** | Optimized production build | `-O3 -DNDEBUG` | ERRORS ONLY (0) | `build/release/spi_controller_test` |
| **ASAN** | Memory error detection | `-fsanitize=address` | INFO (2) | `build/asan/spi_controller_test` |
| **Coverage** | Code coverage analysis | `--coverage` | WARN (1) | `build/coverage/spi_controller_test` |

### Logging Levels
- **0**: Errors only
- **1**: Warnings and errors
- **2**: Info, warnings, errors
- **3**: Full debug/trace output

---

## Building the Model

The SPI Controller model uses **CMake** as its build system.

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
cd models/ot/spi_controller
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
./debug/spi_controller_test
```

**Release Build** - Optimized, errors-only logging:

```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
make
./release/spi_controller_test
```

**ASAN Build** - Memory error detection with AddressSanitizer:

```bash
cmake -DCMAKE_BUILD_TYPE=ASAN ..
make
./asan/spi_controller_test
```

**Coverage Build** - Code coverage analysis:

```bash
cmake -DCMAKE_BUILD_TYPE=Coverage ..
make
make coverage  # Runs tests and generates HTML coverage report
firefox coverage/html/index.html
```

**Static Analysis** - Run cppcheck on model code:

```bash
# From any build directory
make spi_controller_cppcheck
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

Build and run the SPI Controller test:

```bash
cd models/ot/spi_controller
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
./debug/spi_controller_test
```

For other build types:

```bash
./release/spi_controller_test    # Release build
./asan/spi_controller_test       # ASAN build
```

### CCI Configuration

The model supports runtime configuration via JSON files. Configuration files are located in `config/`:

- `spi_controller_default.json` - Default configuration (1 CS, 72-word TX FIFO, 64-word RX FIFO)
- `spi_controller_multi_device.json` - Multi-device configuration (2 chip selects)
- `spi_controller_large_fifo.json` - Large FIFO configuration (128-word FIFOs)

Configuration is automatically loaded if CCI libraries are available.

---

## Configuration Parameters

The SPI Controller model supports the following configuration parameters:

| Parameter | Type | Default | Description | Valid Range |
|-----------|------|---------|-------------|-------------|
| **NumCS** | uint32_t | 1 | Number of chip select lines | 1-8 |
| **TxDepth** | uint32_t | 72 | TX FIFO depth in 32-bit words | 4-256 |
| **RxDepth** | uint32_t | 64 | RX FIFO depth in 32-bit words | 4-256 |
| **ByteOrder** | bool | true | Byte ordering (true=Little-Endian) | true/false |
| **CmdDepth** | uint32_t | 4 | Command queue depth | 1-16 |
| **ClkPeriodNs** | double | 10.0 | Clock period in nanoseconds | >0 |
| **TimeKeeperQuantumNs** | double | 1000.0 | Temporal decoupling quantum (ns) | 0=disabled |

### Example JSON Configuration

```json
{
  "testbench": {
    "spi_controller_dut": {
      "NumCS": 2,
      "TxDepth": 128,
      "RxDepth": 128,
      "ByteOrder": true,
      "CmdDepth": 8,
      "ClkPeriodNs": 10.0,
      "TimeKeeperQuantumNs": 1000.0
    }
  }
}
```

---

## Verification & Testing

The testbench (`test/src/testbench.cpp`) includes comprehensive tests covering:

### Test Suite Overview

| Test ID | Test Function | Description |
|---------|---------------|-------------|
| **test_func000** | Comprehensive Reset | Hardware reset behavior, register initialization |
| **test_func001** | Flash Fast Read Sequence | Typical SPI flash read transaction (command + address + data) |
| **test_func002** | Speed Mode Validation | Standard/Dual/Quad SPI mode switching |
| **test_func003** | FIFO Stall Conditions | TX underflow and RX overflow handling |
| **test_func004** | Interrupt-Driven TX/RX | Event interrupt generation for FIFO watermarks |
| **test_func005** | Error Recovery Flow | Error interrupt generation and recovery |
| **test_func006** | Multi-Device Switching | Chip select switching between multiple devices |
| **test_func007** | Multi-Segment CSAAT | Command Segment with chip select active-after-transaction |
| **test_func008** | Control Flow | CTRL register - SPIEN, SW_RST, OUTPUT_EN |
| **test_func009** | Command Queue Depth | Command queue overflow and management |
| **test_func010** | DMA Trigger | DMA trigger generation based on watermarks |

### Test Coverage Areas

- ✅ **Register Access**: Read/write all registers, field-level access
- ✅ **Reset Behavior**: Hardware reset, software reset
- ✅ **SPI Modes**: Standard, Dual, Quad SPI operation
- ✅ **FIFO Management**: TX/RX FIFO read/write, watermarks, full/empty conditions
- ✅ **Command Queue**: Segment descriptors, command processing, CSAAT
- ✅ **Multi-Device**: Chip select switching, per-device timing configuration
- ✅ **Interrupts**: Error interrupts, event interrupts, interrupt masking
- ✅ **DMA Triggers**: Watermark-based trigger generation
- ✅ **Error Conditions**: Underflow, overflow, programming violations
- ✅ **Timing Control**: Clock divider, delay cycles, chip select timing

### Test Execution

Tests run automatically when you execute the binary:

```bash
./debug/spi_controller_test
```

**Expected Output**: Console logs showing test progress and PASS/FAIL status for each test.

---

## Code Coverage

Generate code coverage reports (model folder only):

```bash
cd models/ot/spi_controller
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
firefox coverage/html/index.html
# or
google-chrome coverage/html/index.html
```

**Note**: Coverage reports include **only model code** (`model/src/*.cpp`), excluding testbench and CSML library code.

---

## Static Analysis

Run `cppcheck` static analysis (model folder only):

```bash
cd models/ot/spi_controller
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make spi_controller_cppcheck
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
- Targets model code only

---

## Documentation

Generate Doxygen documentation:

```bash
cd models/ot/spi_controller
mkdir build && cd build
cmake ..
make docs
```

**View Documentation**:

```bash
firefox docs/html/index.html
```

**Documentation Includes**:
- Class hierarchy and relationships
- API reference for all public/protected methods
- Register definitions and bitfields
- Port and signal descriptions
- Configuration parameters

---

## Cleaning

Remove all build artifacts:

```bash
cd build
make clean
```

Or completely remove the build directory:

```bash
cd models/ot/spi_controller
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
# Build debug version with CCI
make clean
make debug

# Run with default configuration
./Obj/debug/sim.x config/spi_controller_default.json
```

### Release Build for Performance Testing

```bash
make clean
make release
./Obj/release/sim.x config/spi_controller_default.json
```

### Memory Error Checking

```bash
make clean
make asan
./Obj/asan/sim.x config/spi_controller_default.json
```

### Generate Coverage Report

```bash
make clean
make cover
# View report in browser
firefox Obj/cover/results/index.html
```

### Run Static Analysis

```bash
make clean
make cppcheck
cat Obj/debug/cppcheck_report.txt
```

---

## Troubleshooting

### Issue: `SYSTEMC_HOME variable is not set`

**Solution**: Update `makefile.config` with correct SystemC paths:

```makefile
SYSTEMC_HOME=/path/to/your/systemc
SYSTEMC_ARCH=linux64
```

### Issue: `cannot find -lsystemc` or `cannot find -lcci-config`

**Solution**: Verify SystemC is installed and libraries exist:

```bash
ls $SYSTEMC_HOME/lib-linux64/libsystemc.so
ls $SYSTEMC_HOME/lib/libcci-config.so
```

If CCI is not available, use `make nocci` instead.

### Issue: CSML headers not found

**Solution**: Verify CSML exists at `../../utils/csml/`:

```bash
ls ../../utils/csml/inc/
```

### Issue: Coverage report not generated

**Solution**: Ensure `lcov` and `gcov` are installed:

```bash
sudo apt-get install lcov  # Ubuntu/Debian
brew install lcov          # macOS
```

### Issue: CCI configuration not loading

**Solution**:
1. Verify JSON file path is correct
2. Check JSON syntax is valid
3. Ensure hierarchical path matches: `testbench.spi_controller_dut.<param>`
4. Use `make nocci` if CCI is not needed

---

## Register Map Reference

The SPI Controller implements the following register groups:

### Interrupt Registers (Offsets 0x00-0x0C)
| Offset | Register | Type | Description |
|--------|----------|------|-------------|
| `0x00` | `INTR_STATUS` | RW1C | Interrupt status (error, spi_event) |
| `0x04` | `INTR_ENABLE` | RW | Interrupt enable mask |
| `0x08` | `INTR_TEST` | WO | Interrupt test register |

### Control and Status (Offsets 0x10-0x24)
| Offset | Register | Type | Description |
|--------|----------|------|-------------|
| `0x10` | `CTRL` | RW | Control register (SPIEN, SW_RST, watermarks) |
| `0x14` | `STATUS` | RO | Status register (READY, ACTIVE, FIFO levels) |
| `0x18` | `CFG` | RW | Device configuration (timing, polarity, phase) |
| `0x1C` | `CSID` | RW | Chip select ID selection |

### Command and Data (Offsets 0x28-0x30)
| Offset | Register | Type | Description |
|--------|----------|------|-------------|
| `0x28` | `CMD` | WO | Command segment descriptor |
| `0x2C` | `RXDATA` | RO | RX FIFO read data |
| `0x30` | `TXDATA` | WO | TX FIFO write data |

### Error and Event Control (Offsets 0x34-0x3C)
| Offset | Register | Type | Description |
|--------|----------|------|-------------|
| `0x34` | `ERROR_ENABLE` | RW | Error interrupt enable |
| `0x38` | `ERROR_STATUS` | RW1C | Error status (programming violations) |
| `0x3C` | `EVENT_ENABLE` | RW | Event interrupt enable (FIFO, status) |

For detailed register field definitions, see:
- `spi_controller-knowledge-base/registers.md`
- `spi_controller-knowledge-base/spi_controller.rdl`
- Generated Doxygen documentation

---

## Related Documentation

- **OpenTitan SPI Host Specification**: [opentitan.org](https://opentitan.org)
- **SystemC Standard**: [Accellera SystemC](https://www.accellera.org/downloads/standards/systemc)
- **SystemC CCI**: [Accellera CCI](https://www.accellera.org/downloads/standards/cci)
- **CSML Library**: `../../utils/csml/README.md`
- **High-Level Design**: `spi_controller-generated-docs/spi_controller-high-level-design.md`
- **Test Plan**: `spi_controller-generated-docs/spi_controller-test-plan.md`
- **RTL Knowledge Base**: `spi_controller-knowledge-base/spi_controller.md`

---

## Support

For issues or questions:

1. Check the **Troubleshooting** section above
2. Review generated documentation: `make docs`
3. Examine test logs in `Debug_Log.txt`
4. Review configuration examples in `config/` directory
5. Contact the model maintainers

---

## Key Features Summary

- **Multi-Mode SPI**: Standard, Dual, Quad SPI operation
- **Multi-Device**: Up to 8 chip select lines
- **Flexible Configuration**: Runtime JSON configuration (CCI) or static constructor parameters
- **DMA Support**: Automatic trigger generation for FIFO management
- **Interrupt Support**: Error and event interrupt generation
- **Command Queue**: Multi-segment transactions with CSAAT support
- **Timing Control**: Configurable clock divider, delays, and CS timing
- **Comprehensive Testing**: 11 test functions covering all features
- **Code Quality**: Coverage reports, static analysis, Doxygen documentation

---

**Model developed by Vayavya Labs**
