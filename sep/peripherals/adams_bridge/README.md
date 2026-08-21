# Adams Bridge (ABR)

SystemC TLM-2.0 loosely-timed model of the SEP Adams Bridge post-quantum
crypto accelerator. The engine exposes **ML-DSA-87** (FIPS 204) and
**ML-KEM-1024** (FIPS 203) behind one 64 KiB MMIO aperture at SEP local
`0x1094_0000` (`sep_crypto_pkg::abr_rule`).

The register, FSM, interrupt, and Key Vault layers match the software-visible
contract in `abr_reg.rdl`. Cryptographic math is pluggable: the default
backend is PQClean **ML-DSA-87 + ML-KEM-1024** (FIPS 204 / FIPS 203) and is
expected to reproduce NIST ACVP vectors. `abr_shake_backend` remains available
via `set_crypto_backend()` for tests that only need round-trip behaviour.

## Status

| Item | State |
|---|---|
| Register map + CTRL/STATUS handshake | Implemented |
| ML-DSA keygen / sign / verify (+ fused KEYGEN+SIGN) | Implemented (FIPS 204) |
| ML-KEM keygen / encaps / decaps (+ fused KEYGEN+DECAPS) | Implemented (FIPS 203) |
| Interrupts, zeroize, Key Vault sideload | Implemented |
| Key-manager DEST 0x10/0x20/0x40/0x80 dual-share | Implemented (KV entries 0–3) |
| `PCR_SIGN` | `set_pcr_digest()` back-door (no PCR bank) |
| Standalone unit tests (`./run_tests.sh`) | Present (incl. NIST ACVP KATs) |
| NIST-conformant ML-DSA / ML-KEM backend | PQClean (default) |
| Wired into `sep-vp` | `0x1094_0000`, PIC 35/36, KM sockets bound |

`run_all_peripherals.sh` auto-discovers any directory with `run_tests.sh`, so
this IP is included in the SEP orchestrator once the tree is on the branch CI
runs.

## Files

```
include/adams_bridge.h     abr_ip — engines, interrupts, Key Vault, timing
include/abr_base.h         csml_memory register declaration (offsets)
include/abr_register.h     CSML register / bitfield types
include/abr_crypto.h       pluggable crypto backend (FIPS default + SHAKE stand-in)
src/adams_bridge.cpp       LT implementation
src/abr_base.cpp           register construction
src/abr_crypto.cpp         FIPS + SHAKE backends
src/abr_pqc_mldsa.c        PQClean ML-DSA-87 wrappers
src/abr_pqc_mlkem.c        PQClean ML-KEM-1024 wrappers
crypto/pqclean/            vendored PQClean snapshot

test/inc/abr_testbench.h   TLM initiator + firmware-style sequencing
test/src/register_tests.cpp
test/src/mldsa_tests.cpp
test/src/mlkem_tests.cpp
test/src/interrupt_kv_tests.cpp
test/src/nist_kat_tests.cpp
```

## Address

`0x1094_0000 – 0x1094_FFFF` (64 KiB)

## Class

```cpp
class abr_ip : public abr_base
```

## Interface

| Port / socket | Direction | Description |
|---|---|---|
| `memory` target (via `abr_base`) | target | TLM-2.0 32-bit register bus |
| `keymgr_tl_socket` | target | Flat Key Vault sideload (unit tests) |
| `keymgr_mldsa_seed_socket` | target | KM DEST 0x10 dual-share → KV[0] |
| `keymgr_mlkem_d_socket` | target | KM DEST 0x20 dual-share → KV[1] |
| `keymgr_mlkem_z_socket` | target | KM DEST 0x40 dual-share → KV[2] |
| `keymgr_mlkem_msg_socket` | target | KM DEST 0x80 dual-share → KV[3] |
| `clk_i` | `sc_in<double>` | Clock frequency in Hz (latency, not a pin clock) |
| `rst_ni` | `sc_in<bool>` | Active-low reset |
| `intr_abr_error` | `sc_out<bool>` | Aggregated error interrupt (SEP PIC source 35) |
| `intr_abr_notif` | `sc_out<bool>` | Aggregated command-done interrupt (SEP PIC source 36) |

## Building and testing

```bash
./run_tests.sh              # Release build + run
./run_tests.sh --debug      # Debug build
./run_tests.sh --asan       # AddressSanitizer + UBSan
./run_tests.sh --coverage   # coverage report
./run_tests.sh --ctest      # via CTest (verbose)
./run_tests.sh --clean      # clean build dir first
./run_tests.sh --docs       # Doxygen
./run_tests.sh --cppcheck   # static analysis
```

`--asan` and `--coverage` are mutually exclusive.

Platform firmware tests (`sep_abr_csr_test`, `sep_abr_keygen_test`,
`sep_abr_keygen_sign_test`, `sep_abr_kat_test`, `sep_abr_nist_kat_test`,
`sep_abr_km_seed_test`) live under `sw/sep-vp-tests/fw-tests-from-tt-oca-hw/fw/sep/tests/`
and are picked up by that tree's `run_all_tests.sh` / `run_test.sh`. They need a
built `sep-vp` and a RISC-V bare-metal toolchain.

## Documentation

- [Programmer-facing specification](docs/01_adams_bridge_Specification/README.md)
- [Register map](docs/01_adams_bridge_Specification/registers.md)
- [High-level design](docs/02_adams_bridge_HighLevel_Design.md)
- [Test plan](docs/03_adams_bridge_Test_Plan.md)
