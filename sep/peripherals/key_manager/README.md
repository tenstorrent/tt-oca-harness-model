# key_manager

SystemC TLM2.0 model of the Key Manager peripheral.  Exposes two memory-mapped interfaces: a mailbox for SEP–KeyManager command/response exchange, and a KPVLP (Key Provisioning / Key Validation and Loading Protocol) window for key material transfer.

**C++ implementation approach:** The Key Manager does not model an internal co-processor ISS.  Key provisioning, derivation, and validation logic is implemented directly as C++ methods invoked by the b_transport handler.  This gives correct functional behaviour at the register interface without the overhead of a second CPU simulation.

## Files

```
model/inc/key_manager_register.h   Register type definitions
model/inc/key_manager_base.h       TLM socket base
model/inc/key_manager.h            key_manager_model class declaration
model/src/key_manager_base.cpp     Base construction
model/src/key_manager.cpp          Command handling and b_transport handlers

test/inc/testbench.h               Testbench module header
test/inc/key_manager_basetest.h    Base test class
test/inc/key_manager_test.h        Test case declarations
test/src/testbench.cpp             sc_main entry
test/src/key_manager_test.cpp      Test orchestration
test/src/key_manager_func001_test.cpp  }
  ...                                  } Functional test cases (13 total)
test/src/key_manager_func013_test.cpp  }
```

## Addresses

| Region | Base | End | Size |
|---|---|---|---|
| Mailbox (KM_MAILBOX_SEP) | `0x10920000` | `0x1092001B` | 0x1C bytes |
| KPVLP (KM_KPV_KPVLP) | `0x10921000` | `0x10921FFF` | 4 KB |

## Class

```cpp
class key_manager_model : public key_manager_base
```

## Interface

| Port / Socket | Direction | Description |
|---|---|---|
| `keymgr_mb_socket` | target | TLM-2.0 32-bit mailbox register bus |
| `keymgr_kpvlp_socket` | target | TLM-2.0 32-bit KPVLP window bus |
| `intr_o` | `sc_out<bool>` | Mailbox interrupt |
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
./bin/key_manager_test
```

## Documentation

[High-Level Design](docs/design-docs/key_manager-high-level-design.md)
