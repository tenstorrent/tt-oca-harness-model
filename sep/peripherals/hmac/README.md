# hmac

SystemC TLM-2.0 loosely-timed model of the SEP HMAC / SHA-2 accelerator.
Supports plain SHA-2 hashing and keyed HMAC in SHA-256, SHA-384 and SHA-512,
with software-supplied or key-manager-sideloaded keys. Firmware streams message
data into a FIFO; the model computes the digest and raises an interrupt on
completion. Architecture, CSRs, and programming sequences are in the hardware
TRM. This tree has the model, its test plan, and how to run the tests.

A paused hash can be saved and resumed: `hash_stop` publishes the intermediate
state through the `DIGEST` and `MSG_LENGTH` registers, and `hash_continue`
takes it back, so software can interleave independent hashes on one engine.

## Status

| Item | State |
|---|---|
| Register file + CMD/STATUS handshake | Implemented |
| SHA-2 / HMAC engine (in-model `sha2_engine`) | Implemented |
| Save/resume via `hash_stop` / `hash_continue` | Implemented |
| Interrupts, wipe, KM sideload | Implemented |
| Standalone tests | `./run_tests.sh` |
| Wired into `sep-vp` | HMAC window, PIC, KM socket |

## Files

```
include/hmac_base.h        Register map and TLM socket base (regmodel::Memory)
include/hmac_interface.h   hmac_if interface
include/hmac_register.h    Register and bitfield definitions (regmodel::Reg)
include/hmac.h             hmac_ip class declaration
include/sha2_engine.h      SHA-2 core with externally visible chaining state
src/hmac_base.cpp          Base construction and register binding
src/hmac.cpp               Command/FIFO/interrupt logic and b_transport handler
src/sha2_engine.cpp        SHA-256/384/512 compression and padding

test/inc/testbench.h       Testbench module header
test/inc/hmac_basetest.h   Register offsets and reset values
test/inc/hmac_test.h       Bus driver and assertions
test/src/testbench.cpp     Test cases and sc_main entry
test/src/hmac_basetest.cpp Common test infrastructure
test/src/hmac_test.cpp     Bus access and assertion helpers
test/src/basic_tests.cpp   Register read/write and reset tests

doc/index.adoc             VP index
doc/implementation.adoc    SystemC/TLM model
doc/test_plan.adoc         cases + run commands
```

## Address

`0x10911000 – 0x10912FFF`  (0x2000 bytes, HMAC_REG)

## Class

```cpp
class hmac_ip : public hmac_base
```

## Interface

| Port / Socket | Direction | Description |
|---|---|---|
| `target_socket` | target | TLM-2.0 32-bit register bus |
| `keymgr_tl_socket` | target | Private key bus from the key manager (sideload) |
| `intr_hmac_done` | `sc_out<bool>` | Operation complete interrupt |
| `intr_fifo_empty` | `sc_out<bool>` | Message FIFO empty interrupt |
| `intr_hmac_err` | `sc_out<bool>` | Error interrupt |
| `alert_fatal_fault` | `sc_out<bool>` | Fatal fault alert, aggregated into the crypto alert |
| `clk_i` | `sc_in<double>` | Clock frequency, used for timing annotation |
| `rst_ni` | `sc_in<bool>` | Active-low reset |

At platform level the three interrupts land on SEP PIC slots 18, 19 and 20, and
`alert_fatal_fault` is OR-ed with the other crypto blocks onto slot 33.

## Why the SHA-2 core is built in-model

The hash is not delegated to OpenSSL. `hash_continue` has to load an arbitrary
chaining state into the engine, and OpenSSL's supported interface deliberately
does not allow that: EVP exposes no digest mid-state, and the low-level
`SHA256_CTX`/`SHA512_CTX` structures that would are deprecated in OpenSSL 3.x
and disappear entirely from builds configured with `OPENSSL_NO_DEPRECATED_3_0`.
`sha2_engine` therefore keeps the chaining variables and the absorbed bit count
as first-class state, which is what the hardware exposes too.

The HMAC wrapper is built on top of that core as an explicit ipad/opad
construction. It is checked against digests produced by OpenSSL across all three
digest sizes and every supported key length.

## Byte ordering

`MSG_FIFO` is a little-endian byte window. A 32-bit word write places its least
significant byte at the lowest address, and that byte is hashed first, matching
OpenTitan's `prim_packer`. A word write of `0x48656C6C` therefore feeds the byte
stream `6C 6C 65 48`, not the ASCII `"Hell"` it resembles. Setting
`CFG.endian_swap` reverses the assembled word before it enters the FIFO.

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

The test binary exits non-zero if any check fails, and prints a
`TESTBENCH PASSED` / `TESTBENCH FAILED` line as its last output.

## Documentation

- [doc/index.adoc](doc/index.adoc) — VP index
- [doc/implementation.adoc](doc/implementation.adoc) — SystemC/TLM model
- [doc/test_plan.adoc](doc/test_plan.adoc) — standalone cases and run commands
