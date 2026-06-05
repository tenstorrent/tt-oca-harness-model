# csrng

SystemC TLM2.0 model of the Cryptographically Secure Random Number Generator.  Implements NIST SP 800-90A CTR-DRBG using AES-256.

**Standalone model:** In the VP, CSRNG does not connect to `entropy_src` or `edn` at the TLM level.  Any model that needs cryptographic randomness calls OpenSSL APIs directly.  CSRNG's register interface is exercised independently via its own testbench.

## Files

```
model/inc/csrng_register.h            Register type definitions
model/inc/csrng_base.h                TLM socket base
model/inc/csrng.h                     csrng_model class declaration
model/src/csrng_base.cpp              Base construction
model/src/csrng.cpp                   DRBG logic and b_transport handler

test/inc/testbench.h                  Testbench module header
test/inc/csrng_basetest.h             Base test class
test/inc/csrng_test.h                 Test case declarations
test/inc/csrng_test_enhanced.h        Extended test helpers
test/src/testbench.cpp                sc_main entry
test/src/csrng_basetest.cpp           Common test infrastructure
test/src/csrng_test.cpp               Test orchestration
test/src/csrng_func001_drbg_lifecycle.cpp          } Functional test cases
test/src/csrng_func002_pseudorandom_generation.cpp }
test/src/csrng_func003_seed_life_management.cpp    }
test/src/csrng_func005_command_interface_fsm.cpp   }
test/src/csrng_func008_register_callbacks.cpp      }
test/src/csrng_func009_control_configuration.cpp   }
```

## Address

`0x10915000 – 0x109157FF`  (0x800 bytes, DRBG_CSRNG)

## Class

```cpp
class csrng_model : public csrng_base
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
./bin/csrng_test
```

## Documentation

[High-Level Design](docs/design-docs/CRNG-high-level-design.md)
