# otbn

SystemC TLM2.0 model of the OpenTitan Big Number accelerator.  Implements a cryptographic coprocessor for public-key operations (RSA-2048, ECC).  Firmware loads instructions and data into IMEM/DMEM, starts execution, and waits for an interrupt on completion.

**C++ implementation approach:** OTBN does not run a second RISC-V ISS internally.  Each supported algorithm (RSA-2048, ECC, …) is implemented as a standalone C++ class (`otbn_algorithm` and its subclasses in `otbn.h`).  When firmware writes the `CMD.execute` register, the model dispatches to the appropriate C++ algorithm class, computes the result, writes it back to DMEM, and raises the done interrupt.  This gives correct functional behaviour without the overhead of instruction-level emulation.

## Files

```
model/inc/otbn_base.h        Register map and TLM socket base
model/inc/otbn.h             Algorithm C++ class hierarchy (otbn_algorithm, RSA, ECC, …)
model/inc/otbn_ip.h          otbn_ip class declaration
model/src/otbn_base.cpp      Base construction and register binding
model/src/otbn.cpp           Algorithm dispatch and b_transport handler

test/inc/testbench.h         Testbench module header
test/inc/otbn_basetest.h     Base test class
test/inc/otbn_test.h         Test case declarations
test/src/testbench.cpp       sc_main entry
test/src/otbn_basetest.cpp   Common test infrastructure
test/src/otbn_test.cpp       Test orchestration
```

## Address

`0x10900000 – 0x1090BFFF`  (0xC000 bytes — registers + IMEM + DMEM)

## Class

```cpp
class otbn_ip : public otbn_base
```

## Interface

| Port / Socket | Direction | Description |
|---|---|---|
| `target_socket` | target | TLM-2.0 32-bit register + memory bus |
| `intr_done_o` | `sc_out<bool>` | Execution complete interrupt |
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
./bin/otbn_test
```

## Documentation

[High-Level Design](docs/design-docs/otbn-high-level-design.md)
