# aes

SystemC TLM-2.0 loosely-timed model of the SEP AES accelerator. Supports AES-128,
AES-192 and AES-256 encryption and decryption in ECB/CBC/CFB/OFB/CTR, plus
authenticated encryption in GCM.
Architecture, CSRs, and programming sequences are in the hardware TRM.
This tree has the model, its test plan, and how to run the tests.

## Status

| Item | State |
|---|---|
| Register file + CTRL/STATUS handshake | Implemented |
| Cipher engine (OpenSSL EVP) | Implemented |
| GCM phases + NIST vectors | Implemented |
| Sideload, alerts, LC escalate | Implemented |
| Standalone tests | `./run_tests.sh` |
| Wired into `sep-vp` | AES window, KM socket, PIC alerts |

## Files

```
include/aes.h              aes_model
include/aes_base.h         CSML register declaration
include/aes_register.h     RO/WO/RW types
src/                       LT implementation
test/                      standalone bench
doc/index.adoc             VP index
doc/implementation.adoc    SystemC/TLM model
doc/test_plan.adoc         cases + run commands
```

## Address

`0x10910000 – 0x1091008B`  (0x8C bytes, AES_REG)

The last word is `CTRL_GCM_SHADOWED` at offset `0x88`.

## GCM

GCM is not a single cipher call. Software selects a phase in
`CTRL_GCM_SHADOWED.PHASE` and feeds one block at a time, and the module keeps
the GHASH accumulator across phases:

| Phase | Consumes DATA_IN | Produces DATA_OUT | Effect |
| --- | --- | --- | --- |
| `GCM_INIT` | no | no | Derives `H = E(K, 0)` and `S = E(K, J0)` from the key and the IV; clears the accumulator |
| `GCM_RESTORE` | yes | no | Reloads a previously saved accumulator |
| `GCM_AAD` | yes | no | Absorbs additional authenticated data |
| `GCM_TEXT` | yes | yes | Counter-mode encrypt/decrypt, absorbing the ciphertext |
| `GCM_SAVE` | no | yes | Exports the accumulator with `S` added |
| `GCM_TAG` | yes | yes | Absorbs the length block and emits the tag |

Two details are easy to get wrong. The hardware keeps no length counters, so
software supplies the `[len(A)]64 || [len(C)]64` block itself during `GCM_TAG`.
And only a subset of phase transitions is accepted: a rejected request leaves
the phase unchanged rather than reporting an error.

## Class

```cpp
class aes_model : public aes_base
```

## Interface

| Port / Socket | Direction | Description |
|---|---|---|
| `target_socket` | target | TLM-2.0 32-bit register bus |
| `intr_o` | `sc_out<bool>` | Interrupt output |
| `rst_ni` | `sc_in<bool>` | Active-low reset |
| `lc_escalate_en_i` | `sc_in<bool>` | Lifecycle escalation input |

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
./bin/aes_test
```

## Documentation

- [doc/index.adoc](doc/index.adoc) — VP index
- [doc/implementation.adoc](doc/implementation.adoc) — SystemC/TLM model
- [doc/test_plan.adoc](doc/test_plan.adoc) — standalone cases and run commands
