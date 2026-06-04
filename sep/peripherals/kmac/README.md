# kmac

SystemC TLM2.0 model of the Keccak Message Authentication Code accelerator.  Supports SHA-3 (224/256/384/512), SHAKE-128/256, and KMAC (Keccak-based MAC).  Accepts a sideload key from the Key Manager via a dedicated TLM socket.

## Files

```
model/inc/kmac_base.h          Register map and TLM socket base
model/inc/kmac.h               kmac_ip class declaration
model/src/kmac_base.cpp        Base construction and register binding
model/src/kmac.cpp             Keccak engine and b_transport handler

test/inc/testbench.h           Testbench module header
test/inc/kmac_basetest.h       Base test class
test/inc/kmac_test.h           Test case declarations
test/inc/kmac_func001_test.h   }
  ...                          } Functional test case headers
test/inc/kmac_func024_test.h   }
test/src/testbench.cpp         sc_main entry
test/src/kmac_basetest.cpp     Common test infrastructure
test/src/kmac_test.cpp         Test orchestration
test/src/kmac_func001_test.cpp }
  ...                          } Functional test cases (25 total)
test/src/kmac_func025_test.cpp }
```

## Address

`0x10913000 – 0x10913FFF`  (0x1000 bytes, KMAC_REG)

## Class

```cpp
class kmac_ip : public kmac_base
```

## Interface

| Port / Socket | Direction | Description |
|---|---|---|
| `target_socket` | target | TLM-2.0 32-bit register bus |
| `keymgr_tl_socket` | target | Key Manager sideload key input |
| `intr_o` | `sc_out<bool>` | Interrupt output |
| `idle_o` | `sc_out<bool>` | Idle status |
| `lc_escalate_en_i` | `sc_in<bool>` | Lifecycle escalation |
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
./bin/kmac_test
```

## Documentation

[High-Level Design](docs/design-docs/kmac-high-level-design.md)
