# lifecycle_ctrl

SystemC TLM2.0 stub model of the Lifecycle Controller.  Manages the device lifecycle state (TEST, DEV, PROD, RMA, …) and drives feature-enable and escalation signals to dependent peripherals.

**Stub / CCI-configured:** The lifecycle state is not computed from a state machine — it is injected at simulation startup from the eFuse model (which in turn reads it from a CCI parameter).  The model exposes the standard register interface for firmware to read back the current state and configure feature demotion, but the underlying state transitions are controlled entirely through configuration rather than hardware emulation.

## Files

```
model/inc/lifecycle_ctrl_register.h   Register type definitions
model/inc/lifecycle_ctrl_base.h       TLM socket base
model/inc/lifecycle_ctrl.h            lifecycle_ctrl_model class declaration
model/src/lifecycle_ctrl_base.cpp     Base construction
model/src/lifecycle_ctrl.cpp          State read-back and b_transport handler

test/inc/testbench.h                  Testbench module header
test/inc/lifecycle_ctrl_basetest.h    Base test class
test/inc/lifecycle_ctrl_test.h        Test case declarations
test/src/testbench.cpp                sc_main entry
test/src/lifecycle_ctrl_basetest.cpp  Common test infrastructure
test/src/lifecycle_ctrl_test.cpp      Test orchestration
```

## Address

`0x10918000 – 0x10918017`  (0x18 bytes, SEP_LIFECYCLE_CTRL)

## Class

```cpp
class lifecycle_ctrl_model : public lifecycle_ctrl_base
```

## Interface

| Port / Socket | Direction | Description |
|---|---|---|
| `target_socket` | target | TLM-2.0 32-bit register bus |
| `lc_escalate_en_o` | `sc_out<bool>` | Escalation signal to crypto peripherals |
| `rst_ni` | `sc_in<bool>` | Active-low reset |

## CCI Configuration (lifecycle_ctrl_vp.ini)

```ini
och_sep_ss1.lc_ctrl.lc_state : 5   # lifecycle state injected from efuse at reset
```

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
./bin/lifecycle_ctrl_test
```

## Documentation

[High-Level Design](docs/design-docs/lifecycle_ctrl-high-level-design.md)
