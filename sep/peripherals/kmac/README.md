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
| `intr_kmac_done` | `sc_out<bool>` | Operation complete, PIC slot 21 |
| `intr_fifo_empty` | `sc_out<bool>` | Message FIFO drained during absorb, PIC slot 22 |
| `intr_kmac_err` | `sc_out<bool>` | Error reported in `ERR_CODE`, PIC slot 23 |
| `alert_recov_operation_err` | `sc_out<bool>` | Recoverable alert; shadow-register update error |
| `alert_fatal_fault` | `sc_out<bool>` | Fatal alert; requires reset |
| `idle_o` | `sc_out<bool>` | Idle status |
| `lc_escalate_en_i` | `sc_in<bool>` | Lifecycle escalation |
| `rst_ni` | `sc_in<bool>` | Active-low reset |
| `clk_i` | `sc_in<bool>` | Clock |

The three interrupts are separate ports rather than one OR-reduced line because RTL drives
`sep_internal_interrupts[20:22]` independently. Both alert ports feed the platform's
`crypto_alert` OR-reduction, mirroring `sep_crypto.sv`.

`app_export[3]` (KeyMgr, LC_CTRL, ROM_CTRL) is deliberately left unbound at the platform:
`kmac_wrapper.sv` ties `app_i` to zero and leaves `app_o` unused, so SEP has no
hardware-initiated KMAC operations in silicon either. The handlers exist so the interface can be
driven from the unit testbench.

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

The unit suite is 229 test cases. A run reports a summary and exits non-zero on any failure.

## Documentation

- [doc/index.adoc](doc/index.adoc) — VP index
- [doc/implementation.adoc](doc/implementation.adoc) — SystemC/TLM model
- [doc/test_plan.adoc](doc/test_plan.adoc) — standalone cases and run commands
