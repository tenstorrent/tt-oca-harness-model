# aon_timer

SystemC TLM2.0 model of the Always-On Timer peripheral.  Combines a watchdog timer (with bark and bite thresholds) and a wakeup timer, both running from the AON clock domain and surviving main-domain resets.

## Files

```
model/inc/aon_timer_base.h     Register map and TLM socket base
model/inc/aon_timer.h          aon_timer_ip class declaration
model/src/aon_timer_base.cpp   Base construction and register binding
model/src/aon_timer.cpp        Timer logic and b_transport handler

test/inc/testbench.h           Testbench module header
test/inc/aon_timer_basetest.h  Base test class
test/inc/aon_timer_test.h      Test case declarations
test/src/testbench.cpp         sc_main entry
test/src/aon_timer_basetest.cpp  Common test infrastructure
test/src/aon_timer_test.cpp    Test orchestration

doc/index.adoc                 VP set entry (includes the two pages)
doc/implementation.adoc        SystemC/TLM model
doc/test_plan.adoc             cases + run commands
```

## Address

`0x10801000 – 0x10801037`  (0x38 bytes, WDT_TIMER_REG)

## Class

```cpp
class aon_timer_ip : public aon_timer_base
```

## Interface

| Port / Socket | Direction | Description |
|---|---|---|
| `target_socket` | target | TLM-2.0 32-bit register bus |
| `wkup_req_o` | `sc_out<bool>` | Wakeup request to power manager |
| `aon_timer_wdog_bark_intr_o` | `sc_out<bool>` | Watchdog bark interrupt |
| `aon_timer_wdog_bite_rst_req_o` | `sc_out<bool>` | Watchdog bite reset request |
| `rst_n` | `sc_in<bool>` | Main domain reset (active-low) |
| `rst_aon_n` | `sc_in<bool>` | AON domain reset (active-low) |

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
./bin/aon_timer_test
```

## Documentation

Architecture, CSRs, and programming sequences are in the hardware
TRM. This tree documents the SystemC/TLM model:

- [index](doc/index.adoc) — VP set entry
- [implementation](doc/implementation.adoc) — sockets, threads, CCI, gaps
- [test plan](doc/test_plan.adoc) — standalone cases and platform runs
