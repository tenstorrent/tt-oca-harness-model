# mailbox

SystemC TLM2.0 model of the AXI Mailbox.  Provides a dual-FIFO message channel between the host (AXI initiator) and SEP (TL-UL target).  Separate write and read FIFOs each generate threshold and error interrupts.

## Files

```
model/inc/mailbox_register.h     Register type definitions
model/inc/mailbox_base.h         TLM socket base
model/inc/mailbox.h              mailbox_ip class declaration
model/src/mailbox_base.cpp       Base construction and register binding
model/src/mailbox.cpp            FIFO logic and b_transport handler

test/inc/testbench.h             Testbench module header
test/inc/mailbox_basetest.h      Base test class
test/inc/mailbox_test.h          Test case declarations
test/src/testbench.cpp           sc_main entry
test/src/mailbox_basetest.cpp    Common test infrastructure
test/src/mailbox_test.cpp        Test orchestration
test/src/test_mailbox_func001.cpp  }
  ...                              } Functional test cases (7 total)
test/src/test_mailbox_func007.cpp  }
```

## Address

`0x10A00000 – 0x10A0784F`  (0x7850 bytes, AXIL_MAILBOX)

## Class

```cpp
class mailbox_ip : public sc_module
```

## Interface

| Port / Socket | Direction | Description |
|---|---|---|
| `target_socket` | target | TLM-2.0 32-bit register bus (SEP side) |
| `wtirq_o` | `sc_out<bool>` | Write FIFO threshold interrupt |
| `rtirq_o` | `sc_out<bool>` | Read FIFO threshold interrupt |
| `eirq_o` | `sc_out<bool>` | Error interrupt |
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
./bin/mailbox_test
```

## Documentation

[High-Level Design](docs/design-docs/mailbox-high-level-design.md)
