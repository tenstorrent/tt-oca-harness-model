# SEP eFuse Model — Summary

## Overview

The SEP eFuse model is a **SystemC/TLM 2.0 functional model** of the one-time-programmable (OTP) eFuse controller integrated into the Tenstorrent SEP (Security Engine Processor). It exposes a **0x644-byte (1604-byte) MMIO window** through a TLM target socket and implements the register semantics of the physical eFuse controller: shadow registers (boot-time cache of burned fuse contents), interface control registers, and Samsung shim timing registers.

---

## Directory Structure

```
sep_efuse/
├── CMakeLists.txt
├── model/
│   ├── inc/
│   │   ├── sep_efuse.h            # Derived model: callbacks, fuse loading
│   │   ├── sep_efuse_base.h       # Base: register instantiation, memory map
│   │   ├── sep_efuse_register.h   # Register type definitions (csml_reg<N>)
│   │   └── sep_efuse_config.h     # Config struct for factory-burned fuse state
│   └── src/
│       ├── sep_efuse.cpp          # WOSET callbacks, load_fuses(), register_callbacks()
│       └── sep_efuse_base.cpp     # reset_all_registers()
├── test/
│   ├── inc/
│   │   ├── testbench.h            # Test orchestrator (sc_module)
│   │   ├── sep_efuse_basetest.h   # Register offsets and access masks
│   │   └── sep_efuse_test.h       # TLM initiator (register read/write)
│   └── src/
│       ├── testbench.cpp          # 10 test cases + sc_main()
│       ├── sep_efuse_basetest.cpp # Register property database
│       └── sep_efuse_test.cpp     # TLM transport layer
├── build/
│   ├── libsep_efuse_model.a       # Compiled static library
│   └── release/sep_efuse_test    # Compiled test executable
└── knowledge-base/efuse/          # RTL reference, register specs, architecture docs
    ├── README.md
    ├── doc/                       # AsciiDoc architecture and programming guides
    ├── rtl/                       # Verilog RTL reference
    ├── data/                      # Register definitions
    └── testbench/                 # RTL testbench templates
```

---

## Class Hierarchy

```
sc_module
  └── sep_efuse_base    (base: register instantiation, TLM memory binding, reset)
        └── sep_efuse_model    (derived: WOSET callbacks, load_fuses(), config)

sep_efuse_basetest      (TLM initiator socket + register offset constants)
  └── sep_efuse_test    (TLM transport: register_read_32 / register_write_32)

testbench               (sc_module: owns DUT + test driver, runs SC_THREAD)
```

---

## Memory Map (0x000–0x643)

### Shadow Registers (0x000–0x3FC)

These registers cache factory-burned eFuse contents and are loaded from `sep_efuse_config_t` at `end_of_elaboration()`.

| Offset | Register | Width | Access | Description |
|--------|----------|-------|--------|-------------|
| 0x000 | LOCKS_LO | 32 | WOSET | Write/read lock bits for shadow registers [31:0] |
| 0x004 | LOCKS_HI | 32 | WOSET | Write/read lock bits [15:0] + reserved |
| 0x008 | LC_STATE | 32 | WOSET | Lifecycle state [3:0] |
| 0x00C | SBOOT_DIS | 32 | RO | [0] disable_secure_boot |
| 0x010 | TRANSIENT_RMA_EN | 32 | RO | [0] transient_rma_en |
| 0x014 | SIP_DIS_LO | 32 | WOSET | Debug/test feature disable vector (low) |
| 0x018 | SIP_DIS_HI | 32 | WOSET | Debug/test feature disable vector (high) |
| 0x01C | SYS_DIS_LO | 32 | WOSET | System feature disable vector (low) |
| 0x020 | SYS_DIS_HI | 32 | WOSET | System feature disable vector (high) |
| 0x024–0x043 | RMA_SIP_TOKEN[8] | 256b | RO | RMA SIP security token |
| 0x044–0x063 | RMA_CHIPLET_TOKEN[8] | 256b | RO | RMA chiplet security token |
| 0x064–0x083 | CLASS_KEY[8] | 256b | RO | Class key |
| 0x084 | CHIPLET_PUBK_REVOKE | 32 | WOSET | Public key revocation bitmap |
| 0x088–0x0A7 | BL1_VERSION[8] | 256b | WOSET | Bootloader 1 version array |
| 0x0A8–0x0C7 | BL2_VERSION[8] | 256b | WOSET | Bootloader 2 version array |
| 0x0C8–0x0E7 | CHIPLET_UID[8] | 256b | RO | Chiplet unique ID |
| 0x0E8–0x107 | SIP_UID[8] | 256b | RO | SIP unique ID |
| 0x108–0x127 | SYS_UID[8] | 256b | RO | System unique ID |
| 0x128–0x147 | SIP_PUBK[8] | 256b | RO | SIP public key hash |
| 0x148–0x167 | SYS_PUBK[8] | 256b | RO | System public key hash |
| 0x168 | STATUS_RPT | 32 | RO | [1:0] report field |
| 0x16C | SEP_ROM_CTRL | 32 | RO | [0] rom_endianness_ctrl, [5:1] rom_swap_ctrl |
| 0x170–0x193 | SEP_SPI_CTRL, SPI_PHY_* | 32 | RO | SPI configuration fields |
| 0x194–0x1B3 | PUBLIC_KEY_0[8] | 256b | RO | Additional public key 0 |
| 0x1B4–0x1D3 | PUBLIC_KEY_1[8] | 256b | RO | Additional public key 1 |
| 0x1D4–0x3FC | RESERVED_* | — | RO | Reserved arrays |

### Interface Control Registers (0x400–0x418)

| Offset | Register | Reset | Access | Description |
|--------|----------|-------|--------|-------------|
| 0x400 | EFUSE_INTERFACE_CTRL_STATUS | 0x1 | RO | [0] efuse_sense_done (always 1 in VP) |
| 0x404 | EFUSE_WRITE_CTRL | 0x0 | R/W | eFuse write control |
| 0x408 | EFUSE_READ_CTRL | 0x0 | R/W | eFuse read control |
| 0x40C | EFUSE_PROGRAM_INTERFACE_RD_DATA | 0x0 | RO | Program read-back data (hw writes) |
| 0x410 | EFUSE_READ_INTERFACE_RD_DATA | 0x0 | RO | Read interface data (always 0 in VP) |
| 0x414 | EFUSE_READ_REQ_TIMEOUT | 0x800000 | R/W | Read request timeout |
| 0x418 | EFUSE_PROGRAM_REQ_TIMEOUT | 0x800000 | R/W | Program request timeout |

### Samsung Shim Timing Registers (0x600–0x640)

| Offset | Register | Reset | Access | Description |
|--------|----------|-------|--------|-------------|
| 0x600 | EFUSE_CTRL_STATUS | 0x00010108 | R/W | Clock divider and period config |
| 0x604 | EFUSE_CTRL_STATUS_1 | 0x000003E8 | R/W | Additional status/control |
| 0x608–0x640 | EFUSE_TIMING_CTRL_0–14 | Various | R/W | 15 OTP timing parameters (tCS, tRW, tAS, tAH, etc.) |

---

## Register Access Semantics

### WOSET (Write-Set-Only)
Bits can only be set (OR'd); writes that attempt to clear bits are silently ignored. Multiple writes accumulate.

Registers: `LOCKS_LO`, `LOCKS_HI`, `LC_STATE`, `SIP_DIS_LO/HI`, `SYS_DIS_LO/HI`, `CHIPLET_PUBK_REVOKE`, `BL1_VERSION[8]`, `BL2_VERSION[8]`.

Implementation:
```cpp
bool sep_efuse_model::handle_write_LOCKS_LO(uint32_t value) {
    m_locks_lo_val |= value;   // Accumulate set bits
    LOCKS_LO = m_locks_lo_val; // Backdoor write (bypasses write_mask)
    return true;
}
```

### RO (Read-Only)
`write_mask = 0x0`; firmware writes are silently discarded. Values are pre-loaded from `sep_efuse_config_t` at `end_of_elaboration()` via backdoor assignment.

Registers: All security tokens, keys, UIDs, `SBOOT_DIS`, `TRANSIENT_RMA_EN`, `EFUSE_INTERFACE_CTRL_STATUS`, all SPI config, `STATUS_RPT`, `SEP_ROM_CTRL`.

### R/W (Read-Write)
Unrestricted firmware access. `write_mask = 0xffffffff`.

Registers: `EFUSE_WRITE_CTRL`, `EFUSE_READ_CTRL`, timeout registers, all SHIM timing registers.

---

## Configuration Structure (`sep_efuse_config_t`)

Passed to the model constructor to represent factory-burned fuse state:

```cpp
struct sep_efuse_config_t {
    uint32_t lc_state;                  // Lifecycle state [3:0]
    uint32_t sboot_dis;                 // Secure boot disable
    uint32_t transient_rma_en;          // Transient RMA enable
    uint32_t sip_dis_lo, sip_dis_hi;    // SIP feature disable vectors
    uint32_t sys_dis_lo, sys_dis_hi;    // System feature disable vectors
    uint32_t rma_sip_token[8];          // RMA SIP token (256-bit)
    uint32_t rma_chiplet_token[8];      // RMA chiplet token (256-bit)
    uint32_t class_key[8];              // Class key (256-bit)
    uint32_t chiplet_pubk_revoke;       // Public key revocation bitmap
    uint32_t bl1_version[8];            // BL1 version (256-bit)
    uint32_t bl2_version[8];            // BL2 version (256-bit)
    uint32_t chiplet_uid[8];            // Chiplet UID (256-bit)
    uint32_t sip_uid[8];                // SIP UID (256-bit)
    uint32_t sys_uid[8];                // System UID (256-bit)
    uint32_t sip_pubk[8];               // SIP public key hash (256-bit)
    uint32_t sys_pubk[8];               // System public key hash (256-bit)
    uint32_t public_key_0[8];           // Additional public key 0 (256-bit)
    uint32_t public_key_1[8];           // Additional public key 1 (256-bit)
    uint32_t status_rpt;                // Status report field
    uint32_t sep_rom_ctrl;              // ROM control
    uint32_t sep_spi_ctrl_field_en;     // SPI control field enable
    // ... SPI PHY and discovery control fields
};
```

---

## Key Design Patterns

### Fuse Loading via Backdoor
At `end_of_elaboration()`, `load_fuses()` assigns config values directly to register objects, bypassing `write_mask` restrictions. This models the hardware behavior of shadow registers being pre-populated from the physical eFuse array before firmware boots.

### Array Register Vectors
Multi-word fields (256-bit keys, UIDs) use `csml_reg_vector<type, N>`:
```cpp
csml_reg_vector<sep_efuse::BL1_VERSION_type<32>, 8> BL1_VERSION;
```
Each element has an independent WOSET callback registered in a loop.

### TLM 2.0 Target Socket
The model exports `tlm_utils::simple_target_socket<csml_memory<32>, 32>` for connection to APB/AXI bridges in the VP. Byte addresses map to 32-bit word offsets; only aligned 32-bit accesses are supported.

### CSML Framework
- `csml_reg<N>` — register base with read_mask, write_mask, reset_value, write callbacks
- `csml_memory<N>` — flat 32-bit word array with TLM socket binding
- `csml_logger` — structured logging with verbosity level parameter
- `csml_param<T>` — CCI-compatible SystemC parameter wrapper

---

## Build System

Defined in `CMakeLists.txt`:

| Target | Type | Description |
|--------|------|-------------|
| `sep_efuse_model` | STATIC library | Model sources only; links `csml_logger`, `SystemC`, `Threads`, optional CCI |
| `sep_efuse_test` | Executable | Model + testbench; built when `BUILD_TESTS=ON` |
| `coverage` | Custom | Runs tests under gcov, generates HTML report |
| `sep_efuse_cppcheck` | Custom | Runs cppcheck static analysis on model sources |

**Build configurations:**
- `Release` — `-O3 -DNDEBUG`, `CSML_DEFAULT_VERBOSITY=1`
- `Debug` — `-O0 -g`, `CSML_DEFAULT_VERBOSITY=3`
- `ASAN` — Address + UBSan sanitizers, `-O0`
- `Coverage` — gcov instrumentation

**Required compile definitions:** , `SC_ALLOW_DEPRECATED_IEEE_API`

---

## Test Suite

10 test cases in `test/src/testbench.cpp`, driven by an `SC_THREAD` that issues TLM read/write transactions:

| # | Test | What It Verifies |
|---|------|-----------------|
| 1 | `test_fuse_load_ro_registers` | Config values correctly loaded into LC_STATE, SBOOT_DIS, STATUS_RPT, etc. |
| 2 | `test_fuse_load_array_registers` | 256-bit array registers (SIP_PUBK, CHIPLET_UID) loaded element-by-element |
| 3 | `test_ro_write_protection` | Write attempts to RO registers (SBOOT_DIS, CHIPLET_UID) are silently ignored |
| 4 | `test_woset_locks` | LOCKS_LO accumulates bits; written zeros cannot clear previously set bits |
| 5 | `test_woset_sip_dis` | SIP_DIS_LO WOSET bit-accumulation behavior |
| 6 | `test_woset_sys_dis` | SYS_DIS_LO WOSET bit-accumulation behavior |
| 7 | `test_efuse_sense_done` | EFUSE_INTERFACE_CTRL_STATUS reads 0x1 (sense_done=1); writes ignored |
| 8 | `test_efuse_read_ctrl_rw` | EFUSE_READ_CTRL supports full R/W; successive overwrites work |
| 9 | `test_shim_ctrl_reset_values` | SHIM timing registers have correct Samsung OTP spec reset values |
| 10 | `test_shim_ctrl_rw` | SHIM timing registers support full R/W modification |

**Test configuration used:**
```cpp
cfg.lc_state            = 0x1;
cfg.status_rpt          = 0x1;
cfg.chiplet_uid[i]      = 0xA0000000 + i;  // i = 0..7
cfg.sip_pubk[i]         = 0xB0000000 + i;
cfg.bl1_version[i]      = 0xC0000000 + i;
```

---

## Statistics

| Metric | Value |
|--------|-------|
| Total MMIO window | 0x644 bytes (1604 bytes) |
| Shadow register area | 0x400 bytes |
| Interface control area | 0x18 bytes |
| Samsung shim area | 0x40 bytes |
| Total registers | 65+ |
| WOSET registers | 8 groups (including 4 multi-word arrays) |
| RO registers | 20+ groups |
| R/W registers | 15+ |
| Test cases | 10 |
| Model source (~LOC) | ~250 |
| Register definitions (~LOC) | ~765 |
| Test source (~LOC) | ~450 |
| C++ standard | C++17 |
| Simulation framework | SystemC 2.3.x + CSML |
