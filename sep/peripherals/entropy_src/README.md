# entropy_src

SystemC TLM2.0 model of the hardware entropy source.  Implements the health-test and conditioning pipeline registers visible to firmware.

**Standalone model:** In the VP, `entropy_src` does not feed `edn` or `csrng` over a TLM connection.  Models that require cryptographic randomness call OpenSSL APIs directly rather than going through the entropy pipeline.  This model's register interface and health-test status registers are exercised independently via its own testbench.

## Files

```
model/inc/entropy_src_base.h       Register map and TLM socket base
model/inc/entropy_src_interface.h  entropy_src_if interface
model/inc/entropy_src.h            entropy_src_ip class declaration
model/src/entropy_src_base.cpp     Base construction
model/src/entropy_src.cpp          Generation thread and b_transport handler

test/inc/testbench.h               Testbench module header
test/inc/entropy_src_basetest.h    Base test class
test/inc/entropy_src_test.h        Test case declarations
test/src/testbench.cpp             sc_main entry
test/src/entropy_src_basetest.cpp  Common test infrastructure
test/src/entropy_src_test.cpp      Test orchestration
test/src/func001_tests.cpp         }
  ...                              } Functional test cases (8 total)
test/src/func008_tests.cpp         }
```

## Address

`0x10916000 – 0x10916FFF`  (0x1000 bytes, ENTROPY_SOURCE)

## Class

```cpp
class entropy_src_ip : public entropy_src_base, public entropy_src_if
```

## Interface

| Port / Socket | Direction | Description |
|---|---|---|
| `target_socket` | target | TLM-2.0 32-bit register bus |
| `irq_o` | `sc_out<bool>` | Interrupt output |
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
./bin/entropy_src_test
```

## Documentation

[High-Level Design](docs/design-docs/entropy_src-high-level-design.md)
