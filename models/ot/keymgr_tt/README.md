# Key Manager TT — SystemC TLM-2.0 Model

## Overview

`keymgr_tt` is a SystemC TLM-2.0 model of Tenstorrent's Key Manager IP (`keymgr`).
It replaces the original PicoRV32 ISS-based firmware execution with a pure C++ firmware
handler that directly implements the same command protocol used by the hardware ROM.

The model exposes:
- A **mailbox register interface** (TLM target socket, `mailbox_socket`) for SEP ↔ KM communication
- A **KPVLP register interface** (TLM target socket, `kpvlp_socket`) for key/policy vault management
- Four **crypto-engine initiator sockets** (`hmac_key_socket`, `kmac_key_socket`, `aes_key_socket`,
  `otbn_key_socket`) through which the firmware transfers key material using the two-share DPA protocol
- A **DRBG random word callback** (`drbg_fn_t`) injected at construction time; used for key generation
  and engine shredding — this is a C++ callback, not a TLM socket

---

## Features

| Feature | Status |
|---|---|
| Mailbox protocol (inbound + outbound FIFO, CRC-8/ROHC header, CRC-32C payload) | Implemented |
| 11 firmware commands (HW_VER, ROM_VER, SRAM_VER, STAT, RECOV_ACK, KPVLP_SLOT_REQ, KEY_REGISTER, KEY_GENERATE, KEY_TRANSFER, KEY_REVOKE, ENGINE_SHRED) | Implemented |
| Key & Policy Vault (KPV) — 32-slot pool, dest_valid, lock_write, lock_use, extend | Implemented |
| Two-share DPA key delivery to HMAC/KMAC/AES (8 words) and OTBN (12 words) | Implemented |
| KEY_CTRL=1 (key load) / KEY_CTRL=0 (shred/wipe) distinction per rom_sideload.h | Implemented |
| HMAC-SHA256 KDF for CMD_KEY_GENERATE | Not Implemented — uses pure DRBG output (deferred) |
| Engine shred — single zero-write pass per engine (DPA multi-pass random overwrites not modelled) | Implemented (simplified) |
| DRBG random word interface (C++ callback, injected at construction) | Implemented |
| KPVLP_STATUS register (unlock_sep bitmask) | Implemented |
| Mailbox IRQ model (level bits[1:0], sticky W1C bits[4:2], FLUSHED_BY_KM) | Implemented |
| Sequence counter validation (RET_CMD_NOSEQ, CRC-8/ROHC header, CRC-32C payload) | Implemented |
| Emergency wipe (wipe_ni) — KPV reset, engine key clear, FLUSHED_BY_KM IRQ to SEP | Implemented |
| SRAM firmware loading | Not Implemented (see TLM Limitations) |
| Post-revoke wipe-done notification to SEP after CMD_KEY_REVOKE | Not Implemented (see TLM Limitations) |

---

## TLM Limitations

### Not Implemented

1. **SRAM firmware loading** — `m_sram_loaded` is always `false`; `CMD_SRAM_VER` always
   returns `RET_FAILURE`.  The C++ firmware handler (`km_firmware_handler`) is
   instantiated directly; there is no ROM/SRAM image loader.

2. **Post-revoke wipe-done notification** — After `CMD_KEY_REVOKE` the slot is invalidated
   and zeroed in the model, but the hardware's asynchronous "wipe complete" interrupt back
   to SEP is not modelled.  Tests that care about post-revoke state must poll `CMD_STAT`
   or rely on the synchronous `RET_SUCCESS` return.
   Note: the emergency wipe path (`wipe_ni` assertion) IS fully modelled — it wipes
   the KPV, shreds all four engine key stores with KEY_CTRL=0, and asserts
   FLUSHED_BY_KM IRQ (MB_IRQS bit 4) to SEP.

3. **HMAC-SHA256 KDF for CMD_KEY_GENERATE** — key material is filled with pure DRBG
   output; there is no KDF derivation step.  The KDF is deferred to a future release.
   Tests should not rely on any relationship between the generated key and OTP/UID inputs.

4. **DPA multi-pass random shred** — `CMD_ENGINE_SHRED` and the boot/wipe key-clear paths
   perform a single pass of zero writes (`KEY_CTRL=0`) to each engine's sideload register
   file.  The hardware performs `KEY_SHRED_ITER` passes of pseudorandom data; the extra
   passes are DPA countermeasures invisible to SEP and are not modelled.

5. **DRBG hardware timing** — The real DRBG has finite latency and a reseed count.
   The model uses `RAND_bytes` (OpenSSL) as its random source.  Tests requiring
   deterministic DRBG output must inject a custom `drbg_fn_t` callback at construction.

6. **SEP base-address VP integration** — HMAC/KMAC/AES/OTBN base addresses are hard-coded
   in `km_firmware_handler`.  In a full VP integration these must match the platform
   memory map.  See `keymgr_tt-knowledge-base/key_manager_mmap/` for the RTL register map.

7. **lock_use one-shot policy** — After the first `CMD_KEY_TRANSFER` the slot's `lock_use`
   flag is set and `km_read_key_word` returns zero for all subsequent reads.  A second
   transfer succeeds (`RET_SUCCESS`) but delivers zero key material to the engine
   (SHARE0 == SHARE1 for every word).  This matches the expected hardware behaviour but
   is listed as pending customer confirmation (item 10 in
   `model/Items to Confirm from Customer.ini`).

### Rationale

The model is intended as a **functional verification model** for SEP firmware integration
and key-management policy tests, not as a cycle-accurate RTL replica.  The omissions above
are accepted trade-offs to keep the model portable and self-contained.

### Impact

- `CMD_SRAM_VER` always returns `-1`; FUNC003 test 003c verifies this behaviour.
- `CMD_KEY_GENERATE` returns a randomly-generated key (no KDF binding to OTP/UID).
- Post-revoke SEP interrupt behaviour cannot be tested from this model alone;
  emergency wipe (wipe_ni) interrupt behaviour can be tested.
- Engine base addresses must be updated before integrating with `riscv-vp-plusplus`.

---

## Fully Implemented Features

### Mailbox Protocol

- **CRC-8/ROHC** header (polynomial 0x07, reflected = 0xE0, init 0xFF, XorOut 0x00;
  computed over header bits [23:0])
- **CRC-32C Castagnoli** payload (polynomial 0x82F63B78 reflected, init = 0xFFFFFFFF,
  XorOut = 0xFFFFFFFF; check value 0xE3069283)
- Separator-terminated framing: header-only messages use a single WSEP+WDATA cycle;
  payload messages append a CRC-32C word with separator
- Sequence counter: validated per message; increment on all paths except
  `RET_HEADER_CRC` (header corrupt) and `RET_CMD_NOSEQ` (sequence mismatch)
- Outbound FIFO with `OUTBOUND_READ_DATA_AVAIL` IRQ (level) and sticky
  `OUTBOUND_UNDERFLOW` (W1C); OUTBOUND_READ_DATA_AVAIL is asserted immediately
  after boot so SEP is notified of `RESP_KM_READY` via IRQ

### Firmware Commands

| Command | ID | Description |
|---|---|---|
| `CMD_HW_VER`        | 0x00 | Returns hardware version (1.0.0) |
| `CMD_ROM_VER`       | 0x01 | Returns ROM version (1.0.0) |
| `CMD_SRAM_VER`      | 0x02 | Always returns `RET_FAILURE` (SRAM not loaded) |
| `CMD_STAT`          | 0x03 | Returns ret_arg[0]=recoverable_err (1 if fault pending) |
| `CMD_RECOV_ACK`     | 0x04 | Clears recoverable fault flag; no return argument |
| `CMD_KPVLP_SLOT_REQ`| 0x20 | Grants 1–8 contiguous KPV slots; sets `unlock_sep` bits |
| `CMD_KEY_REGISTER`  | 0x21 | Validates CRC, locks slot for write, marks valid |
| `CMD_KEY_GENERATE`  | 0x22 | Fills slot with DRBG output (no KDF); allocates handle |
| `CMD_KEY_TRANSFER`  | 0x24 | Delivers key to engine via two-share DPA (KEY_CTRL=1) |
| `CMD_KEY_REVOKE`    | 0x23 | Invalidates and zeroes slot; frees handle |
| `CMD_ENGINE_SHRED`  | 0x25 | Sends one zero-write pass to engine sideload (KEY_CTRL=0) |

Boot completion is announced as `RESP_KM_READY` (0x55), a zero-payload outbound message
pushed before the firmware enters the message loop.

### Key & Policy Vault

- 32-slot pool (`km_kpv::NUM_SLOTS = 32`), 512 bits per slot (`WORDS_PER_KEY = 16`)
- Per-slot fields: `key_word[16]`, `dest_valid`, `last_dword`, `extend`, `lock_write`,
  `lock_use`, `m_valid`
- `CMD_KPVLP_SLOT_REQ` sets `unlock_sep` (visible in `KPVLP_STATUS`) without locking
  the slot; only `CMD_KEY_REGISTER` / `CMD_KEY_GENERATE` permanently reserve a slot
- `KPVLP_KEY` registers are write-only (read returns 0)

### Two-Share DPA Engine Key Delivery

`key_transfer_via_socket(dest_mask, words, count, set_valid)` implements the
`rom_sideload.c` DPA protocol.  For each targeted engine:

```
actual = min(count, max_words)
SHARE0[0..actual-1]   @ base + word×4              = DRBG random mask
SHARE1[0..actual-1]   @ base + share1_base + word×4 = key[w] XOR SHARE0[w]
KEY_CTRL              @ base + key_ctrl_off          = 1 (key load) or 0 (shred/wipe)
```

Total writes per call: `2 × actual + 1`

| Engine | max_words | share1_base | key_ctrl_off | writes/call |
|---|---|---|---|---|
| HMAC | 8 | 0x20 | 0x40 | 17 |
| KMAC | 8 | 0x20 | 0x40 | 17 |
| AES  | 8 | 0x20 | 0x40 | 17 |
| OTBN | 12 | 0x30 | 0x60 | 25 |

**KEY_CTRL value by call site:**

| Call site | set_valid | KEY_CTRL written | Effect |
|---|---|---|---|
| `CMD_KEY_TRANSFER` | `true` | 1 | Engine key valid (keymgr_key_i.valid=1) |
| `CMD_ENGINE_SHRED` | `false` | 0 | Engine key invalid (keymgr_key_i.valid=0) |
| `boot()` init | `false` | 0 | Engine key invalid at startup |
| `wipe_ni` assert | `false` | 0 | Emergency clear, engine key invalid |

---

## Directory Structure

```
models/ot/keymgr_tt/
├── CMakeLists.txt                        # Build system (Debug/Release/Coverage)
├── GAP_ANALYSIS.md                       # Known gaps vs. RTL specification
├── README.md                             # This file
│
├── keymgr_tt-knowledge-base/            # Reference material
│   ├── KeyManager.md                     # Hardware architecture overview
│   ├── KM_FW.md                          # Firmware protocol specification
│   └── key_manager_mmap/                 # Register map in multiple formats
│       ├── adoc/                         # AsciiDoc source
│       ├── c/                            # C header register definitions
│       ├── htm/                          # HTML renderings
│       ├── py_headers/                   # Python header structs
│       ├── rdl/                          # SystemRDL source
│       └── rtl/                          # RTL parameter files
│
├── model/
│   ├── Items to Confirm from Customer.ini   # 15 open questions for hardware team
│   ├── inc/
│   │   ├── keymgr_tt.h                   # Top-level SC_MODULE declaration
│   │   ├── keymgr_tt_model.h             # Wrapper / test-facing interface
│   │   ├── km_firmware_handler.h         # Firmware command handler (command IDs,
│   │   │                                 #   return codes, dest constants)
│   │   ├── km_kpv.h                      # Key & Policy Vault declaration
│   │   └── km_mailbox.h                  # Mailbox FIFO and framing declaration
│   └── src/
│       ├── keymgr_tt.cpp                 # Top-level: socket bindings, key_transfer_via_socket
│       ├── keymgr_tt_model.cpp           # Wrapper implementation
│       ├── km_firmware_handler.cpp       # All 11 firmware command handlers
│       ├── km_kpv.cpp                    # KPV slot management
│       └── km_mailbox.cpp                # Inbound/outbound FIFO, framing, IRQ
│
└── test/
    ├── inc/
    │   ├── keymgr_tt_test.h              # Test helper class + recording_engine_stub
    │   ├── keymgr_tt_basetest.h          # Register-offset helpers (kpvlp_key_offset etc.)
    │   └── testbench.h                   # Top-level testbench (4 stubs, drbg_initiator)
    └── src/
        ├── keymgr_tt_test.cpp            # mb_send_command, mb_receive_frame, CRC helpers
        ├── testbench.cpp                 # Socket bindings, run_tests dispatcher
        ├── keymgr_tt_func001_test.cpp    # FUNC001: boot sequence and initialization
        ├── keymgr_tt_func002_test.cpp    # FUNC002: mailbox IRQ and WSEP mechanics
        ├── keymgr_tt_func003_test.cpp    # FUNC003: version and status commands
        ├── keymgr_tt_func004_test.cpp    # FUNC004: protocol error handling
        ├── keymgr_tt_func005_test.cpp    # FUNC005: CMD_KPVLP_SLOT_REQ
        ├── keymgr_tt_func006_test.cpp    # FUNC006: CMD_KEY_REGISTER
        ├── keymgr_tt_func007_test.cpp    # FUNC007: CMD_KEY_GENERATE (DRBG)
        ├── keymgr_tt_func008_test.cpp    # FUNC008: CMD_KEY_TRANSFER (DPA protocol)
        ├── keymgr_tt_func009_test.cpp    # FUNC009: CMD_KEY_REVOKE
        ├── keymgr_tt_func010_test.cpp    # FUNC010: CMD_ENGINE_SHRED
        ├── keymgr_tt_func011_test.cpp    # FUNC011: full key lifecycle
        └── keymgr_tt_func012_test.cpp    # FUNC012: reset state verification
```

---

## Building

### Prerequisites

| Dependency | Minimum Version | Notes |
|---|---|---|
| CMake | 3.16 | |
| C++ compiler | C++17 | GCC 9+ or Clang 10+ |
| SystemC | 3.0 | Set `SYSTEMC_HOME` |
| OpenSSL | 1.1 | `libssl-dev` — required for `RAND_bytes` (DRBG source) |
| CCI | optional | Set `CCI_HOME` if available |

### Environment Setup

```bash
export SYSTEMC_HOME=/path/to/systemc
export OPENSSL_ROOT_DIR=/usr/        # or wherever libssl is installed
# Optional:
export CCI_HOME=/path/to/cci
```

### Build Types

```bash
cd models/ot/keymgr_tt

# Debug (default — assertions enabled, -O0 -g)
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug -j$(nproc)

# Release (-O2 -DNDEBUG)
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release -j$(nproc)

# Coverage (Debug + --coverage)
cmake -S . -B build/coverage -DCMAKE_BUILD_TYPE=Coverage
cmake --build build/coverage -j$(nproc)
```

### Build Options

| CMake Option | Default | Description |
|---|---|---|
| `CMAKE_BUILD_TYPE` | `Debug` | `Debug`, `Release`, or `Coverage` |
| `SYSTEMC_HOME` | (env) | Path to SystemC installation |
| `CCI_HOME` | (env) | Path to CCI installation (optional) |
| `OPENSSL_ROOT_DIR` | (env) | Path to OpenSSL installation |

---

## Running Tests

The testbench binary is built as `build/<type>/keymgr_tt_testbench`.
CTest is configured to run it as a single test target.

```bash
# Run all FUNC tests via CTest
cd build/debug
ctest --output-on-failure

# Or run the binary directly for verbose output
./keymgr_tt_testbench
```

Expected output summary per test:

```
--- FUNC001: Boot Sequence ---
[PASS] 001a: ...
...
--- FUNC001 complete: 0 failure(s) ---

--- FUNC012: Reset State ---
[PASS] 012a: ...
...
--- FUNC012 complete: 0 failure(s) ---
```

### Coverage Report

```bash
cd build/coverage
cmake --build . --target coverage
# HTML report generated in build/coverage/coverage_html/
```

---

## Additional Targets

```bash
# Clean build artifacts
cmake --build build/debug --target clean

# List all available targets
cmake --build build/debug --target help
```

---

## Integration as Subproject

Add the `keymgr_tt` directory as a CMake subdirectory in a parent project:

```cmake
# In parent CMakeLists.txt
add_subdirectory(models/ot/keymgr_tt)
target_link_libraries(my_platform PRIVATE keymgr_tt_model)
```

The `keymgr_tt_model` CMake target exports the `model/inc/` include directory.

---

## Model Usage

### Instantiation and Socket Binding

```cpp
#include "keymgr_tt_model.h"

// In SC_MODULE or sc_main:
keymgr_tt_model km("km");

// Bind mailbox register interface (SEP → KM)
sep_bus.bind(km.mailbox_socket);      // TLM target socket (mailbox registers, 7 × 32-bit)

// Bind KPVLP register interface
sep_bus.bind(km.kpvlp_socket);        // TLM target socket (KPVLP registers, 0x884 bytes)

// Bind crypto-engine interfaces (KM → engines)
km.hmac_key_socket.bind(hmac_model.key_socket);
km.kmac_key_socket.bind(kmac_model.key_socket);
km.aes_key_socket.bind(aes_model.key_socket);
km.otbn_key_socket.bind(otbn_model.key_socket);

// Inject DRBG random word source (C++ callback — no TLM socket)
km.set_drbg_callback([]() -> uint32_t { return my_drbg.next_word(); });

// Optional: push OTP data after end_of_elaboration
km.set_otp_data(otp_data);

// Optional: wire live demotion-state from lc_ctrl at start_of_simulation
km.set_demote_callback([&lc]() { return lc.get_demote_state(); });
```

### Register Map (Mailbox, base = 0x00)

| Offset | Register | Access | Reset | Description |
|---|---|---|---|---|
| 0x00 | MB_WDATA  | WO        | 0x00000000 | Write 32-bit word to inbound FIFO |
| 0x04 | MB_WSEP   | RW bit[0] | 0x00000000 | Arm separator on next WDATA write; self-clears |
| 0x08 | MB_RDATA  | RO        | 0x00000000 | Read 32-bit word from outbound FIFO |
| 0x0C | MB_STATUS | RO+W1C    | 0x00000005 | FIFO status and depth (see below) |
| 0x10 | MB_IRQS   | RO+W1C    | 0x00000000 | IRQ status (see below) |
| 0x14 | MB_IRQEN  | RW        | 0x00000000 | IRQ enable mask (bits[4:0]) |
| 0x18 | MB_CTRL   | RW        | 0x00000000 | bit[2]=FLUSH (self-clearing), bits[1:0]=overflow/underflow resp. |

`MB_IRQS` bit layout:

| Bit | Name | Type | Description |
|---|---|---|---|
| 0 | OUTBOUND_READ_DATA_AVAIL | Level | Outbound FIFO non-empty (high while data available) |
| 1 | INBOUND_WRITE_SPACE_AVAIL | Level | Inbound FIFO not full (high while space available) |
| 2 | INBOUND_OVERFLOW | W1C | SEP wrote to full inbound FIFO |
| 3 | OUTBOUND_UNDERFLOW | W1C | SEP read from empty outbound FIFO |
| 4 | FLUSHED_BY_KM | W1C | KM firmware flushed both FIFOs (fatal fault or wipe) |

### KPVLP Register Map (base = kpvlp_socket base)

| Offset | Register | Access | Description |
|---|---|---|---|
| 0x000–0x7FF | KPVLP_KEY[32][16] | WO | 32 slots × 16 words × 4 bytes; write-only, read returns 0 |
| 0x800–0x87C | KPVLP_CTRL[32]    | RW | Per-slot ctrl: EXTEND[6:4], DEST_VALID[16:9], LAST_DWORD[20:17] |
| 0x880       | KPVLP_STATUS      | RO | Bit i = unlock_sep for slot i (32-bit bitmask) |

### Message Framing

**Header word** (32-bit):

```
[7:0]   SEQ       — sequence number
[15:8]  CMD_ID    — command identifier
[23:16] PAY_LEN   — number of 32-bit payload words (0 = header-only)
[31:24] CRC8      — CRC-8/ROHC over bits [23:0] (poly 0xE0 reflected, init 0xFF)
```

**Frame layout:**

- Header-only: `WSEP=1`, `WDATA=header`
- With payload: `WDATA=header`, `WDATA=payload[0]`, ..., `WDATA=payload[N-1]`,
  `WSEP=1`, `WDATA=CRC32C`

**RESP_CMD payload encoding** (response from KM → SEP):

```
word[0] = src_seq  (uint32)   — echoed sequence number
word[1] = cmd_id   (uint32)   — echoed command ID
word[2] = ret_code (int32)    — return code (see table below)
word[3] = ret_arg  (uint32)   — command-specific result (optional, present when ret_code >= 0
                                or for certain error conditions per KM_FW.md)
```

Return code values (from `km_firmware_handler::ret_code_t`):

| Value | Name | Meaning |
|---|---|---|
|  0 | `RET_SUCCESS`     | Command succeeded |
| -1 | `RET_FAILURE`     | General failure (slot full, invalid handle, etc.) |
| -2 | `RET_HEADER_CRC`  | Header CRC-8 mismatch |
| -3 | `RET_CMD_NOSEQ`   | Sequence number out of order |
| -4 | `RET_INVALID_CMD` | Unknown command ID |
| -5 | `RET_INVALID_LEN` | Payload length wrong for command |
| -6 | `RET_PAYLOAD_CRC` | Payload CRC-32C mismatch |
| -7 | `RET_INVALID_ARG` | Argument value out of range or policy violation |

---

## Knowledge Base

Reference documentation for the hardware specification is in `keymgr_tt-knowledge-base/`:

| File / Directory | Contents |
|---|---|
| `KeyManager.md` | Hardware architecture, state machine, lifecycle |
| `KM_FW.md` | Firmware protocol specification (commands, framing, error codes) |
| `key_manager_mmap/c/` | C header register definitions (generated from RDL) |
| `key_manager_mmap/rdl/` | SystemRDL register map source |
| `key_manager_mmap/rtl/` | RTL parameter files |
| `key_manager_mmap/adoc/` | AsciiDoc register documentation |

Open design questions are tracked in `model/Items to Confirm from Customer.ini`
(15 items as of the last update).

---

## Build Configurations Summary

| Configuration | Flags | Use case |
|---|---|---|
| Debug | `-O0 -g -DDEBUG` | Development, assertions active |
| Release | `-O2 -DNDEBUG` | Integration, performance |
| Coverage | `-O0 -g --coverage` | CI coverage reporting |

---

## Compiler Flags

| Flag | Applied in | Purpose |
|---|---|---|
| `-std=c++17` | All | Required by model sources |
| `-Wall -Wextra` | All | General warnings |
| `-Werror` | Debug | Warnings as errors in development builds |
| `--coverage` | Coverage | gcov instrumentation |

---

## Dependencies

| Library | Link target | Notes |
|---|---|---|
| SystemC | `${SYSTEMC_LIBRARIES}` | SC_MODULE, TLM-2.0 |
| OpenSSL (crypto) | `OpenSSL::Crypto` | `RAND_bytes` for DRBG random source |
| CCI (optional) | `${CCI_LIBRARIES}` | Parameter infrastructure |

---

## Troubleshooting

**`OpenSSL not found` during CMake configure**
Set `OPENSSL_ROOT_DIR` to your OpenSSL prefix, e.g.:
```bash
cmake -S . -B build/debug -DOPENSSL_ROOT_DIR=/usr/local/opt/openssl
```

**`SystemC library not found`**
Ensure `SYSTEMC_HOME` points to a directory containing `lib/libsystemc.a` (or `.so`)
and `include/systemc.h`.

**`[FAIL] 007b: HMAC stub received 2×8+1=17 writes`** after engine model changes
If you replace a recording stub with a real engine model, verify that the engine's
target socket accepts TLM_WRITE transactions at the SHARE0, SHARE1, and KEY_CTRL offsets.
The model always writes exactly `2 × min(count, max_words) + 1` words per call.
For key loads KEY_CTRL=1; for shred/wipe/boot-init KEY_CTRL=0.

**Test hangs in `mb_receive_frame`**
Default timeout is 2000 ns of simulated time.  If the firmware handler is not responding,
check that `trigger_reset()` was called at the start of the test and that the SystemC
scheduler is advancing (no delta-cycle deadlock).

**`CMD_SRAM_VER` always returns `-1`**
This is expected — SRAM firmware loading is not implemented.  See TLM Limitations above.

**`CMD_KEY_GENERATE` returns unexpected key material**
Key material is pure DRBG output — there is no KDF.  Do not rely on any relationship
between the generated key and OTP/UID values.  See TLM Limitations above.

**SEP does not receive `RESP_KM_READY` via IRQ**
`OUTBOUND_READ_DATA_AVAIL` (MB_IRQS bit 0) is asserted immediately after boot completes.
Ensure MB_IRQEN bit 0 is set before deasserting reset, or poll MB_STATUS bit 2
(OUTBOUND_EMPTY) as a fallback.

---

---

## References

- `keymgr_tt-knowledge-base/KeyManager.md` — Hardware architecture specification
- `keymgr_tt-knowledge-base/KM_FW.md` — Firmware mailbox protocol specification
- `keymgr_tt-knowledge-base/key_manager_mmap/` — Register map (RDL / C headers / RTL)
- [OpenSSL RAND_bytes documentation](https://www.openssl.org/docs/man3.0/man3/RAND_bytes.html)
/home/pavank/tenztorrent/__release__/tenstorrent_sep/models/ot/keymgr_tt/README.md