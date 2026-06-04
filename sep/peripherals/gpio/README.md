# gpio

SystemC TLM2.0 model of the General Purpose I/O peripheral.  Controls pad direction, drive strength, pull configuration, and Schmitt trigger enable.  Supports both direct GPIO access and LSIO (Low-Speed IO bridge) routing.

## Files

```
model/inc/gpio_base.h        Register map and TLM socket base
model/inc/gpio.h             gpio_ip class declaration
model/src/gpio_base.cpp      Base construction and register binding
model/src/gpio.cpp           Pin logic and b_transport handler

test/inc/testbench.h         Testbench module header
test/inc/gpio_basetest.h     Base test class
test/inc/gpio_test.h         Test case declarations
test/src/testbench.cpp       sc_main entry
test/src/gpio_basetest.cpp   Common test infrastructure
test/src/gpio_test.cpp       Test orchestration
```

## Address

`0x46010000 – 0x46010FFF`

## Class

```cpp
class gpio_ip : public gpio_base
```

## Interface

| Port / Socket | Direction | Description |
|---|---|---|
| `target_socket` | target | TLM-2.0 32-bit register bus |
| `gpio_out_o` | `sc_out<bool>` | GPIO output value |
| `gpio_oe_o` | `sc_out<bool>` | GPIO output enable |
| `gpio_in_i` | `sc_in<bool>` | GPIO input value |
| `lsio_gpio_out_i` | `sc_in<bool>` | LSIO bridge output |
| `lsio_gpio_oe_i` | `sc_in<bool>` | LSIO bridge output enable |
| `lsio_gpio_in_o` | `sc_out<bool>` | LSIO bridge input feedback |
| `lsio_access_i` | `sc_in<bool>` | LSIO ownership select |
| `pad_drive_strength_o` | `sc_out<sc_uint<3>>` | Pad drive strength |
| `pad_pull_enable_o` | `sc_out<bool>` | Pad pull enable |
| `pad_pull_select_o` | `sc_out<bool>` | Pull-up / pull-down select |
| `pad_schmitt_enable_o` | `sc_out<bool>` | Schmitt trigger enable |
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
./bin/gpio_test
```

## Documentation

[High-Level Design](docs/design-docs/gpio-high-level-design.md)
