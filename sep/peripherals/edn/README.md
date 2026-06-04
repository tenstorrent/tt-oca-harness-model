# edn

SystemC TLM2.0 model of the Entropy Distribution Network.  Acts as a bridge between the hardware entropy source and entropy consumers, managing reseed scheduling and distributing entropy words on demand.

**Standalone model:** In the VP, EDN does not connect to `entropy_src` or `csrng` at the TLM level.  Models that need randomness call OpenSSL APIs directly.  EDN's register interface is exercised independently via its own testbench.

## Files

```
model/inc/edn_base.h             Register map and TLM socket base
model/inc/edn.h                  edn_ip class declaration
model/src/edn_base.cpp           Base construction and register binding
model/src/edn.cpp                Distribution logic and b_transport handler

test/inc/testbench.h             Testbench module header
test/inc/edn_basetest.h          Base test class
test/inc/edn_test.h              Test case declarations
test/inc/test_edn_func_001.h     }
  ...                            } Functional test case headers
test/inc/test_edn_func_014.h     }
test/src/testbench.cpp           sc_main entry
test/src/edn_basetest.cpp        Common test infrastructure
test/src/edn_test.cpp            Test orchestration
test/src/test_edn_func_001.cpp   }
  ...                            } Functional test cases (14 total)
test/src/test_edn_func_014.cpp   }
```

## Address

`0x10915800 – 0x10915847`  (0x48 bytes, DRBG_EDN)

## Class

```cpp
class edn_ip : public edn_base
```

## Interface

| Port / Socket | Direction | Description |
|---|---|---|
| `target_socket` | target | TLM-2.0 32-bit register bus |
| `intr_o` | `sc_out<bool>` | Interrupt output |
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
./bin/edn_test
```

## Documentation

[High-Level Design](docs/design-docs/edn-high-level-design.md)
