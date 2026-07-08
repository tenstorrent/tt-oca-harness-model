# spi_flash

SystemC model of an SPI NOR flash device.  Implements the standard SPI flash command set (WREN, READ, PROGRAM, ERASE, READ_SFDP, SUSPEND/RESUME).  Connected to `spi_controller` via the `spi_if` interface — it has no direct memory-mapped bus address.

**Split implementation:** The model is layered into two parts — `spi_flash_model` (pure C++, no SystemC) handles all flash logic and is covered by a standalone unit test; `spi_flash` (sc_module) wraps it and is tested via a SystemC testbench.  This keeps the core logic testable without a simulator.

## Files

```
model/inc/spi_flash_model.h       Pure C++ flash core (spi_flash_model class)
model/inc/spi_flash_sfdp.h        SFDP table structure definitions
model/inc/spi_flash.h             SystemC wrapper (spi_flash sc_module)
model/src/spi_flash_model.cpp     Command handler, program/erase/read logic
model/src/spi_flash.cpp           sc_module wiring and spi_if implementation

test/inc/spi_flash_sfdp_utils.h   SFDP test utilities
test/src/spi_flash_sfdp_utils.cpp SFDP parser / pretty-printer
test/src/spi_flash_test.cpp       Pure C++ unit tests (no sc_main)
test/src/spi_flash_sc_test.cpp    SystemC integration testbench (sc_main)
```

## Class

```cpp
class spi_flash : public sc_module, public spi_if
```

## Interface

| Port / Export | Direction | Description |
|---|---|---|
| `spi_target` | `sc_export<spi_if>` | SPI transaction target (bound by spi_controller) |
| `rst_ni` | `sc_in<bool>` | Active-low reset — clears WEL, preserves memory |

## Behavior notes

**Multi-segment (CSAAT-chained) reads.** A read command is framed as one opcode+address TX segment followed by one or more RX segments; when the driver keeps CS asserted (`csaat=1`) across several RX segments, the model serves each RX segment's data from a running address that advances by each segment's length. (Previously data was served only on the final `csaat=0` segment, which truncated multi-segment reads.) A bare RX segment with no preceding opcode+address returns `0xFF` (idle MISO).

## Building and Testing

```bash
# Using run_tests.sh (recommended)
./run_tests.sh              # Release build + run both tests
./run_tests.sh --debug      # Debug build
./run_tests.sh --asan       # AddressSanitizer
./run_tests.sh --coverage   # lcov coverage report
./run_tests.sh --ctest      # via CTest (verbose)
./run_tests.sh --clean      # clean build dir first

# Manual CMake
mkdir -p build/debug && cd build/debug
cmake ../.. -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
./bin/spi_flash_test        # pure C++ unit tests
./bin/spi_flash_sc_test     # SystemC integration tests
```
