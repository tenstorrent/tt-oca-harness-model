# efuse

SystemC TLM2.0 stub model of the eFuse / OTP shim.  Provides read access to fuse-burned device identity fields — lifecycle state, chiplet UID, and key metadata.

**Stub / CCI-configured:** There is no physical fuse simulation.  All fuse field values are injected at simulation startup via CCI parameters in the `.ini` config file.  Writes to the register interface are ignored.  The lifecycle controller reads fuse state from this model at reset to determine the current device lifecycle.

## Files

```
model/inc/efuse_base.h        Register map and TLM socket base
model/inc/efuse.h             efuse_model class declaration
model/src/efuse_base.cpp      Base construction and register binding
model/src/efuse.cpp           CCI parameter binding and b_transport handler

test/inc/testbench.h          Testbench module header
test/inc/efuse_basetest.h     Base test class
test/inc/efuse_test.h         Test case declarations
test/src/testbench.cpp        sc_main entry
test/src/efuse_basetest.cpp   Common test infrastructure
test/src/efuse_test.cpp       Test orchestration
```

## Address

`0x10930000 – 0x10930643`  (EFUSE_SHIM_CTRL)

## Class

```cpp
class efuse_model : public efuse_base
```

## Interface

| Port / Socket | Direction | Description |
|---|---|---|
| `target_socket` | target | TLM-2.0 32-bit register bus |
| `rst_ni` | `sc_in<bool>` | Active-low reset |

## CCI Configuration (efuse_vp.ini)

Key fields injected at runtime — no recompile needed:

```ini
och_sep_ss1.sep_efuse.lc_state   : 5   # lifecycle state value
och_sep_ss1.sep_efuse.chiplet_uid : 0x0001AABBCCDDEEFF
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
./bin/efuse_test
```

## Documentation

[High-Level Design](docs/design-docs/efuse-high-level-design.md)
