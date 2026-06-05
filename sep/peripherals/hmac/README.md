# hmac

SystemC TLM2.0 model of the HMAC / SHA-256 accelerator.  Supports standalone SHA-256 hashing and HMAC-SHA-256 message authentication.  Firmware writes message data into a streaming FIFO; the model computes the digest and raises an interrupt on completion.

## Files

```
model/inc/hmac_base.h        Register map and TLM socket base
model/inc/hmac_interface.h   hmac_if interface
model/inc/hmac.h             hmac_ip class declaration
model/src/hmac_base.cpp      Base construction and register binding
model/src/hmac.cpp           SHA-256/HMAC engine and b_transport handler

test/inc/testbench.h         Testbench module header
test/inc/hmac_basetest.h     Base test class
test/inc/hmac_test.h         Test case declarations
test/src/testbench.cpp       sc_main entry
test/src/hmac_basetest.cpp   Common test infrastructure
test/src/hmac_test.cpp       Test orchestration
test/src/basic_tests.cpp     Functional test cases
```

## Address

`0x10911000 – 0x10912FFF`  (0x2000 bytes, HMAC_REG)

## Class

```cpp
class hmac_ip : public hmac_base
```

## Interface

| Port / Socket | Direction | Description |
|---|---|---|
| `target_socket` | target | TLM-2.0 32-bit register bus |
| `intr_hmac_done_o` | `sc_out<bool>` | Operation complete interrupt |
| `intr_fifo_empty_o` | `sc_out<bool>` | Message FIFO empty interrupt |
| `intr_hmac_err_o` | `sc_out<bool>` | Error interrupt |
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
./bin/hmac_test
```

## Documentation

[High-Level Design](docs/design-docs/hmac-high-level-design.md)
