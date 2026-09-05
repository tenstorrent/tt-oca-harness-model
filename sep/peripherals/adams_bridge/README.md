# Adams Bridge (ABR)

SystemC TLM-2.0 loosely-timed model of the SEP Adams Bridge accelerator.
Architecture, CSRs, and programming sequences are in the hardware TRM.
This tree has the model, its test plan, and how to run the tests.

## Status

| Item | State |
|---|---|
| Register file + CTRL/STATUS handshake | Implemented |
| ML-DSA / ML-KEM command engines | Implemented (PQClean FIPS default) |
| Interrupts, zeroize, Key Vault, KM DEST sockets | Implemented |
| `PCR_SIGN` | `set_pcr_digest()` back-door (no PCR bank) |
| Standalone tests | `./run_tests.sh` (includes NIST ACVP KATs) |
| Wired into `sep-vp` | ABR window, PIC error/notif, KM sockets |

## Files

```
include/adams_bridge.h     abr_ip
include/abr_base.h         regmodel register declaration
include/abr_register.h     RO/WO/RW types
include/abr_crypto.h       pluggable backend
src/                       LT implementation + PQClean wrappers
crypto/pqclean/            vendored PQClean snapshot
test/                      standalone bench
doc/implementation.adoc    SystemC/TLM model
doc/test_plan.adoc         cases + run commands
```

## Building and testing

```bash
./run_tests.sh              # Release build + run
./run_tests.sh --debug
./run_tests.sh --asan       # AddressSanitizer + UBSan
./run_tests.sh --coverage   # do not combine with --asan
./run_tests.sh --ctest
./run_tests.sh --clean
```

Platform firmware tests on `sep-vp`:

```bash
cd sw/sep-vp-tests/fw-tests-from-tt-oca-hw/fw/sep
./run_test.sh sep_abr_csr_test
./run_test.sh sep_abr_nist_kat_test
./run_test.sh sep_abr_km_seed_test
```

Those need a built `sep-vp` and a RISC-V bare-metal toolchain. Full
commands are in `doc/test_plan.adoc`.
