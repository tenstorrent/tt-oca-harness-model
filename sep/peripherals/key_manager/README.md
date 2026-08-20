# key_manager

SystemC TLM2.0 model of the Key Manager peripheral.  Exposes a single memory-mapped interface: a mailbox for SEP–KeyManager command/response exchange.  Everything else in the block — the key vault, the DRBG sampler, and the crypto-engine key wrappers — sits behind the KM's internal CPU and is not addressable from SEP.

**C++ implementation approach:** The Key Manager does not model an internal co-processor ISS.  The behaviour of the ROM firmware running on the block's CPU is implemented directly as C++ command handlers driven by the mailbox.  This gives correct functional behaviour at the register interface without the overhead of a second CPU simulation.

Key material reaches the vault through the `CMD_KEY_LOAD` mailbox command.  Earlier revisions of the hardware exposed a second SEP-visible KPVLP window for this purpose; that window no longer exists, and the commands that went with it (`0x20`, `0x21`) now report `RET_INVALID_CMD`.

## Key provisioning

`CMD_KEY_GENERATE` (0x22) and `CMD_KEY_LOAD` (0x26) are the same operation with different sources of key material: generate draws it from the DRBG, load takes it from the SEP payload.  Both go through one `load_key()` helper, mirroring `rom_load_key()` in firmware, which is what keeps the two commands from drifting apart.

`KEY_SIZE` in both payloads is a **word count minus one**, not a slot count.  A key occupies `ceil(len / 16)` consecutive vault slots, up to 128 words across 8 slots.  The base slot records the span in `EXTEND`; the final slot records its last populated word in `LAST_DWORD`, and those two fields alone are what the length is later recovered from.

Provisioning follows the firmware's ordering, which matters:

1. Reject a bad length or an empty `DEST_VALID` — a key with no permitted destination could never be used.
2. Find a run of consecutive available slots, starting from a DRBG-random index.
3. CRC-32C the key material as supplied.
4. **Register the handle before touching the vault**, so a registry-exhaustion failure cannot strand a written, write-locked slot with no handle referring to it.
5. Erase the run, write the key, then write-lock the whole span.

Handles are issued monotonically and never recycled: revoking a key returns its slots to the pool but not its handle number, and the registry is spent after 255 allocations regardless of how much vault space is free.

A message may be longer than the 16-word mailbox FIFO.  SEP writes it in instalments and the KM drains as it goes, reassembling in a buffer sized like the firmware's SRAM message buffer; a 128-word key is a 131-word frame and arrives across nine fills.

## Files

```
include/key_manager_register.h   Register type definitions
include/key_manager_base.h       TLM socket base
include/key_manager.h            key_manager_model class declaration
include/km_kpv.h                 Internal key & policy vault
include/km_firmware_handler.h    KM CPU / ROM firmware command handlers
src/key_manager_base.cpp         Base construction
src/key_manager.cpp              Integration and b_transport handlers
src/km_kpv.cpp                   Vault storage, scrambling, erase
src/km_firmware_handler.cpp      Mailbox command dispatch

test/inc/testbench.h               Testbench module header
test/inc/key_manager_basetest.h    Base test class
test/inc/key_manager_test.h        Test case declarations
test/src/testbench.cpp             sc_main entry
test/src/key_manager_test.cpp      Test orchestration
test/src/key_manager_func001_test.cpp  }
  ...                                  } Functional test cases (14 total)
test/src/key_manager_func014_test.cpp  }
```

## Addresses

| Region | Base | End | Size |
|---|---|---|---|
| Mailbox (KM_MAILBOX_SEP) | `0x10920000` | `0x1092001B` | 0x1C bytes |

`0x10921000` is unmapped and accessing it raises a bus error.

## Class

```cpp
class key_manager_model : public key_manager_base
```

## Interface

| Port / Socket | Direction | Description |
|---|---|---|
| `mailbox_socket` | target | TLM-2.0 32-bit mailbox register bus |
| `hmac_key_socket`, `kmac_key_socket`, `aes_key_socket`, `otbn_key_socket` | initiator | Sideload of a vault key into a crypto engine |
| `irq` | `sc_out<bool>` | Mailbox interrupt |
| `rst_ni` | `sc_in<bool>` | Active-low reset |
| `wipe_ni` | `sc_in<bool>` | Active-low emergency wipe |

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
./bin/key_manager_test
```

## Documentation

- [Specification](docs/01_key_manager_Specification/) — the hardware team's own RTL, firmware and register documentation, vendored. Specification of record.
- [High-Level Design](docs/02_key_manager_HighLevel_Design.md) — this model's design.
- [Test Plan](docs/03_key_manager_Test_Plan.md) — what the IP-level suite covers.
