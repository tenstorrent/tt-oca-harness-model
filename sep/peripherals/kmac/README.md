# kmac

SystemC TLM-2.0 loosely-timed model of the SEP KMAC accelerator.
Architecture, CSRs, and programming sequences are in the hardware TRM.
This tree has the model, its test plan, and how to run the tests.

## Status

| Item | State |
|---|---|
| Register file + CMD FSM | Implemented |
| SHA-3 / SHAKE / cSHAKE / KMAC (OpenSSL) | Implemented |
| KM sideload, app exports, interrupts | Implemented |
| Standalone tests | `./run_tests.sh` |
| Wired into `sep-vp` | KMAC window, PIC, KM socket (`app_export` unbound) |

## Files

```
include/kmac.h             kmac_ip
include/kmac_base.h        CSML register declaration
include/kmac_register.h    RO/WO/RW types
include/kmac_interface.h   kmac_app_if
src/                       LT implementation
test/                      standalone bench
doc/index.adoc             VP index
doc/implementation.adoc    SystemC/TLM model
doc/test_plan.adoc         cases + run commands
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
./bin/kmac_test
```

## Documentation

- [doc/index.adoc](doc/index.adoc) — VP index
- [doc/implementation.adoc](doc/implementation.adoc) — SystemC/TLM model
- [doc/test_plan.adoc](doc/test_plan.adoc) — standalone cases and run commands
