# secure_dma

SystemC TLM2.0 model of the Secure DMA controller.  Transfers data between memory regions under firmware control while enforcing access-policy restrictions tied to lifecycle state.

## Files

```
model/inc/secure_dma_base.h        Register map and TLM socket base
model/inc/secure_dma.h             secure_dma_model class declaration
model/src/secure_dma_base.cpp      Base construction and register binding
model/src/secure_dma.cpp           Transfer engine and b_transport handler

test/inc/testbench.h               Testbench module header
test/inc/secure_dma_basetest.h     Base test class
test/inc/secure_dma_test.h         Test case declarations
test/src/testbench.cpp             sc_main entry
test/src/secure_dma_basetest.cpp   Common test infrastructure
test/src/secure_dma_test.cpp       Test orchestration
test/src/test_dma_func_001.cpp     }
  ...                              } Functional test cases (12 total)
test/src/test_dma_func_012.cpp     }

doc/index.adoc                     VP set entry (includes the two pages)
doc/implementation.adoc            SystemC/TLM model
doc/test_plan.adoc                 cases + run commands
```

## Address

`0x10800000 – 0x1080014F`  (0x150 bytes, SECURE_DMA)

## Class

```cpp
class secure_dma_model : public secure_dma_base
```

## Interface

| Port / Socket | Direction | Description |
|---|---|---|
| `target_socket` | target | TLM-2.0 32-bit register bus |
| `init_socket` | initiator | TLM-2.0 bus master for DMA transfers |
| `intr_o` | `sc_out<bool>` | Transfer complete / error interrupt |
| `rst_ni` | `sc_in<bool>` | Active-low reset |

## Behavior notes

**Hardware-handshake RX drain (peripheral-to-memory).** When `CONTROL.hardware_handshake_enable` (bit 4) is set, arming the transfer (`CONTROL.go`) does **not** move any data immediately. The engine waits for the first RX-watermark trigger on `lsio_trigger` before draining the first chunk, then drains one chunk per trigger until `TOTAL_DATA_SIZE` is reached. This matches hardware: the DMA is typically armed before the peripheral read is issued, so draining on arm would read an empty peripheral FIFO. Non-handshake (memory-to-memory) transfers still start immediately on `go`.

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
./bin/secure_dma_test
```

## Documentation

Architecture, CSRs, and programming sequences are in the hardware
TRM. This tree documents the SystemC/TLM model:

- [index](doc/index.adoc) — VP set entry
- [implementation](doc/implementation.adoc) — sockets, threads, CCI, gaps
- [test plan](doc/test_plan.adoc) — standalone cases and platform runs
