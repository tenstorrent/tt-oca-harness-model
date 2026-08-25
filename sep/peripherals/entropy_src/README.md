# entropy_src

SystemC TLM-2.0 loosely-timed model of the SEP hardware entropy source.
Architecture, CSRs, and programming sequences are in the hardware TRM.
This tree has the model, its test plan, and how to run the tests.

In `sep-vp`, `entropy_src` does not feed `edn` or `csrng` over a TLM
connection. Models that need randomness call OpenSSL directly. The
register interface is exercised by this testbench.

## Status

| Item | State |
|---|---|
| Register file + FIFO | Implemented |
| Background OpenSSL filler thread | Implemented |
| Combined `irq_o` | Implemented |
| Health tests | Register stubs |
| Standalone tests | `./run_tests.sh` |
| Wired into `sep-vp` | ENTROPY_SOURCE window, PIC (no EDN bind) |

## Files

```
include/entropy_src.h            entropy_src_ip
include/entropy_src_base.h       CSML register declaration
include/entropy_src_register.h   RO/WO/RW types
include/entropy_src_interface.h  callback contract
src/                             LT implementation
test/                            standalone bench
doc/index.adoc                   VP index
doc/implementation.adoc          SystemC/TLM model
doc/test_plan.adoc               cases + run commands
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
./bin/entropy_src_test
```

## Documentation

- [doc/index.adoc](doc/index.adoc) — VP index
- [doc/implementation.adoc](doc/implementation.adoc) — SystemC/TLM model
- [doc/test_plan.adoc](doc/test_plan.adoc) — standalone cases and run commands
