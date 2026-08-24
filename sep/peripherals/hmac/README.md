# hmac

SystemC TLM-2.0 loosely-timed model of the SEP HMAC / SHA-2 accelerator.
Architecture, CSRs, and programming sequences are in the hardware TRM.
This tree has the model, its test plan, and how to run the tests.

## Status

| Item | State |
|---|---|
| Register file + CMD/STATUS handshake | Implemented |
| SHA-2 / HMAC engine (OpenSSL) | Implemented |
| Interrupts, wipe, KM sideload | Implemented |
| Standalone tests | `./run_tests.sh` |
| Wired into `sep-vp` | HMAC window, PIC, KM socket |

## Files

```
include/hmac.h             hmac_ip
include/hmac_base.h        CSML register declaration
include/hmac_register.h    RO/WO/RW types
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
./bin/hmac_test
```

## Documentation

- [doc/index.adoc](doc/index.adoc) — VP index
- [doc/implementation.adoc](doc/implementation.adoc) — SystemC/TLM model
- [doc/test_plan.adoc](doc/test_plan.adoc) — standalone cases and run commands
