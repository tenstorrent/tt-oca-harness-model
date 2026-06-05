# aes

SystemC TLM2.0 model of the AES hardware accelerator.  Supports AES-128 and AES-256 encryption and decryption in ECB/CBC/CTR modes via a memory-mapped register interface.

## Files

```
model/inc/aes_base.h           Register map and TLM socket base
model/inc/aes.h                aes_model class declaration
model/src/aes_base.cpp         Base construction and register binding
model/src/aes.cpp              Cipher engine and b_transport handler

test/inc/testbench.h           Testbench module header
test/inc/aes_basetest.h        Base test class
test/inc/aes_test.h            Test case declarations
test/src/testbench.cpp         sc_main entry
test/src/aes_basetest.cpp      Common test infrastructure
test/src/aes_test.cpp          Test orchestration
test/src/aes_func001_test.cpp  } Functional test cases
test/src/aes_func00N_test.cpp  }
```

## Address

`0x10910000 – 0x10910087`  (0x88 bytes, AES_REG)

## Class

```cpp
class aes_model : public aes_base
```

## Interface

| Port / Socket | Direction | Description |
|---|---|---|
| `target_socket` | target | TLM-2.0 32-bit register bus |
| `intr_o` | `sc_out<bool>` | Interrupt output |
| `rst_ni` | `sc_in<bool>` | Active-low reset |
| `lc_escalate_en_i` | `sc_in<bool>` | Lifecycle escalation input |

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
./bin/aes_test
```

## Documentation

[High-Level Design](docs/design-docs/aes-high-level-design.md)
