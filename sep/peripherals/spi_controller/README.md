# spi_controller

SystemC TLM2.0 model of the SPI master controller.  Paired with `spi_flash` to form the subsystem.  Firmware programs transfer descriptors into memory-mapped registers; the controller drives the `spi_if` socket to execute SPI transactions on the attached flash device.

## Files

```
model/inc/spi_controller_interface.h  spi_if and segment type definitions
model/inc/spi_controller_base.h       Register map and TLM socket base
model/inc/spi_controller.h            spi_controller_ip class declaration
model/src/spi_controller_base.cpp     Base construction and register binding
model/src/spi_controller.cpp          Transaction engine and b_transport handler

test/inc/testbench.h                  Testbench module header
test/inc/spi_controller_basetest.h    Base test class
test/inc/spi_controller_test.h        Test case declarations
test/src/testbench.cpp                sc_main entry
test/src/spi_controller_basetest.cpp  Common test infrastructure
test/src/spi_controller_test.cpp      Test orchestration
test/src/test_func000.cpp             }
  ...                                 } Functional test cases (11 total)
test/src/test_func010.cpp             }
test/src/test_coverage.cpp            Coverage-oriented regression
```

## Address

`0x10B00000 – 0x10B00037`  (0x38 bytes, SPI_CONTROLLER_REG)

## Class

```cpp
class spi_controller_ip : public spi_controller_base
```

## Interface

| Port / Socket | Direction | Description |
|---|---|---|
| `target_socket` | target | TLM-2.0 32-bit register bus |
| `spi_port` | `sc_port<spi_if>` | SPI transaction interface to flash |
| `intr_o` | `sc_out<bool>` | Transfer complete interrupt |
| `rst_ni` | `sc_in<bool>` | Active-low reset |

## Building and Testing

```bash
# Using run_tests.sh (recommended)
./run_tests.sh              # Release build + run
./run_tests.sh --debug      # Debug build
./run_tests.sh --asan       # AddressSanitizer
./run_tests.sh --coverage   # lcov coverage report
./run_tests.sh --ctest      # via CTest (verbose)
./run_tests.sh --clean      # clean build dir first
./run_tests.sh --cppcheck   # for static analysis

# Manual CMake
mkdir -p build/debug && cd build/debug
cmake ../.. -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
./bin/spi_controller_test
```

## Documentation

[High-Level Design](docs/design-docs/spi_controller-high-level-design.md)
