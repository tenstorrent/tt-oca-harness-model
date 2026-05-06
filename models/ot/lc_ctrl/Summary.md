# lc_ctrl — Lifecycle Controller Model Summary

## Overview

`lc_ctrl` is a **SystemC TLM-2.0 functional model** of the Tenstorrent SEP (Secure Enclave Processor) Lifecycle Controller (`tt_sep_lifecycle_ctrl`). It mirrors the RTL logic in `lc_ctrl-knowledge-base/tt_sep_lifecycle_ctrl.sv` and provides a simulation-ready C++ model for integration testing and VP (Virtual Platform) work.

The controller's job is straightforward: given the device lifecycle state (read from OTP fuses) and a set of fuse-programmed disable masks, compute a 64-bit `FEAT_CTRL` output that gates debug, test, and functional features on the chip.

---

## Directory Structure

```
lc_ctrl/
├── CMakeLists.txt              Build configuration
├── Summary.md                  This file
├── csml/                       Register simulation library (CSML)
│   ├── inc/                    Headers: register, memory, clock, test, logging
│   └── src/                    Implementations
├── docs/
│   └── sections/
│       └── lc_ctrl-register-map.csv   Register map source-of-truth
├── lc_ctrl-knowledge-base/     Reference RTL and register specs
│   ├── sep_lifecycle_ctrl.rdl  Register definition (RDL)
│   ├── sep_lifecycle_ctrl.htm  Auto-generated HTML docs
│   ├── tt_sep_lifecycle_ctrl.sv            Top-level RTL
│   ├── tt_sep_lifecycle_ctrl_reg.sv        Generated register wrapper
│   ├── tt_sep_lifecycle_ctrl_reg_inner.sv  Generated register internals
│   ├── tt_sep_lifecycle_ctrl_reg_pkg.sv    Generated SV package
│   └── otp_lcc_reg.h           Full OTP/LCC register address map (~5100 lines)
├── model/
│   ├── inc/
│   │   ├── lc_ctrl.h           Top-level model class (lifecycle FSM + FEAT_CTRL logic)
│   │   ├── lc_ctrl_base.h      Base class: registers + TLM socket
│   │   └── lc_ctrl_register.h  Register type definitions (CSML templates)
│   └── src/
│       └── lc_ctrl_base.cpp    reset_all_registers() implementation
└── test/
    ├── inc/
    │   ├── lc_ctrl_basetest.h  Register offsets, masks, reset values; TLM initiator
    │   └── lc_ctrl_test.h      Derived test class with 8-bit read/write helpers
    └── src/
        └── lc_ctrl_basetest.cpp   Register property map (reg_map[3])
```

---

## Register Map

| Register      | Offset | Width | SW Access | Reset      | Description |
|---------------|--------|-------|-----------|------------|-------------|
| `FEAT_CTRL_LO` | 0x0000 | 32    | RO        | 0x00000000 | Lower 32 bits of feature control — written by hardware |
| `FEAT_CTRL_HI` | 0x0004 | 32    | RO        | 0x00000000 | Upper 32 bits of feature control — written by hardware |
| `DEMOTE`       | 0x1000 | 32    | RW (W1S)  | 0x00000000 | Demote control: bit[0]=demote (swwe-gated W1S), bit[1]=lock (W1S) |

`FEAT_CTRL_LO` and `FEAT_CTRL_HI` together form a 64-bit feature-enable vector. Both are hardware-written and software-read-only.

---

## Lifecycle States

The model encodes five lifecycle states via a 4-bit `lc_state` value (sourced from `fuse_map.LC_STATE`):

| State       | Encoding  | Feature Control Behavior |
|-------------|-----------|--------------------------|
| `TEST_DEV`  | `4'b0000` | All features enabled except those masked by `sop_dis \| sys_dis` |
| `PROD`      | `4'b0001` | All debug disabled; if `demote=1` → PROD_DBG (same as TEST_DEV) |
| `PROD_END`  | `4'b1000` | No debug; functional features follow fuse mask |
| `RMA_SoP`   | `4'b001x` | All features enabled except `sop_dis` mask |
| `RMA_CHIPLET`| `4'b01xx` | All features fully enabled (0xFFFFFFFF_FFFFFFFF) |

A `security_disable` input overrides all states and forces full feature enablement.

---

## Class Hierarchy

```
sc_module
└── lc_ctrl_base          (csml_memory + 3 registers + TLM target socket)
    └── lc_ctrl           (lifecycle FSM + FEAT_CTRL compute + DEMOTE callback)
```

### `lc_ctrl_base` (`model/inc/lc_ctrl_base.h`)
- Owns a `csml_memory<32>` and a `tlm_utils::simple_target_socket`.
- Instantiates `FEAT_CTRL_LO`, `FEAT_CTRL_HI`, `DEMOTE` register objects.
- Provides `reset_all_registers()`.

### `lc_ctrl` (`model/inc/lc_ctrl.h`)
Key members:

| Member | Type | Purpose |
|---|---|---|
| `lc_state_shadow` | `uint32_t` | LC state from OTP fuse |
| `sop_dis_lo/hi` | `uint32_t` | SoP feature disable mask (from fuse) |
| `sys_dis_lo/hi` | `uint32_t` | System feature disable mask (from fuse) |
| `security_disable` | `bool` | Override: enables all features unconditionally |
| `secure_tm` | `bool` | When false, clears test-mode bits in FEAT_CTRL |
| `demote_swwe` | `bool` | Software write-enable gate for the demote bit |

Key methods:

| Method | Description |
|---|---|
| `set_shadow_regs(lc, sop_lo, sop_hi, sys_lo, sys_hi)` | Load OTP shadow values and recompute FEAT_CTRL |
| `compute_feat_ctrl()` | Core logic — mirrors RTL combinational block; writes FEAT_CTRL_LO/HI |
| `on_demote_write(value)` | Write callback: enforces W1S, lock check, swwe gating, triggers recompute |

### `lc_ctrl_register.h`
CSML template register types inside `namespace lc_ctrl`:
- `FEAT_CTRL_LO_type<N>` — read-mask `0xFFFFFFFF`, write-mask `0x0` (RO)
- `FEAT_CTRL_HI_type<N>` — same as above
- `DEMOTE_type<N>` — read/write-mask `0xFFFFFFFF`; fields: `demote[0]`, `lock[1]`, `reserved0[31:2]`

---

## DEMOTE Register Semantics

The `DEMOTE` register implements hardware-enforced write-1-set (W1S) semantics:

1. **Lock check** — if `lock` (bit 1) is set, all writes are silently ignored.
2. **Demote bit (bit 0)** — can only be set (not cleared); requires `demote_swwe == true`.
3. **Lock bit (bit 1)** — can only be set (not cleared); no `swwe` gate.
4. After any accepted write, `compute_feat_ctrl()` is called to update `FEAT_CTRL`.

This mirrors `tt_sep_lifecycle_ctrl_reg_inner.sv` behavior exactly.

---

## CSML Library

CSML (C++ System Modelling Library) is a thin abstraction layer over SystemC/TLM providing:

| Module | File | Role |
|---|---|---|
| Register | `csml_register.h` | `csml_reg<N>`, `csml_bitfield<N>`, `csml_memory<N>`, register vectors |
| Descriptor | `csml_descriptor.h` | Virtual register/field group management |
| Clock | `csml_clock.h` | Clock source/receiver, posedge/negedge observer pattern |
| Logger | `csml_logger.h` | Severity-based logging with `CSML_REPORT` macro |
| Test | `csml_test.h` | `CSML_ASSERT*` macros and test-result aggregation |
| Router | `csml_router.h` | TLM address router for multi-bank register maps |
| Parameter | `csml_parameter.h` | CCI/SCML parameter integration |

---

## Build System

Built with CMake (≥ 3.14), C++17. Produces:

| Artifact | Type | Contents |
|---|---|---|
| `liblc_ctrl_model.a` | Static library | `model/src/` compiled objects |
| `lc_ctrl_testbench` | Executable | `test/src/` + `csml/src/` + linked against model and SystemC |

**Dependencies:** SystemC (default `/usr/local/systemc300`; override via `SYSTEMC_HOME` env var or CMake variable), pthreads.

Build flags: `-DUSE_CSML_REG_LIB -DSC_ALLOW_DEPRECATED_IEEE_API`, warnings via `-Wall -Wextra`.

---

## Test Infrastructure

`lc_ctrl_basetest` provides:
- A `tlm_utils::simple_initiator_socket` for driving TLM transactions.
- Enums for all register offsets, read masks, write masks, and reset values.
- A `Register_Property_t` struct and `reg_map[3]` array used by test cases.

`lc_ctrl_test` extends `lc_ctrl_basetest` with byte-granularity `register_read_8` / `register_write_8` helpers.

---

## Reference RTL (Knowledge Base)

| File | Purpose |
|---|---|
| `sep_lifecycle_ctrl.rdl` | Authoritative register spec (used to generate SV wrappers) |
| `tt_sep_lifecycle_ctrl.sv` | Top-level RTL with APB4 interface and LC state machine |
| `tt_sep_lifecycle_ctrl_reg_inner.sv` | Bus protocol + W1S register field implementation |
| `otp_lcc_reg.h` | Full SEP OTP/LCC register address map (base: `SEP_LIFECYCLE_CTRL = 0x01100000`) |

The C++ model in `lc_ctrl.h:compute_feat_ctrl()` directly corresponds to the combinational `always_comb` block in `tt_sep_lifecycle_ctrl.sv`.

---

## Key Design Points

- **No SC_THREAD / SC_METHOD** — the model is purely combinational: shadow registers are set by the testbench and `compute_feat_ctrl()` runs synchronously on each update. There is no clock-driven process.
- **FEAT_CTRL is read-only to software** — write-mask is `0x0`; the model writes these directly bypassing the mask.
- **DEMOTE is the only software-writable register**, and only the two LSBs are meaningful.
- **`security_disable` is a C++ member, not a register** — it must be set before simulation starts (or via a testbench method call); it has no TLM-mapped address.
- The `secure_tm` gating (masking test bits from FEAT_CTRL when `secure_tm=false`) is stubbed — the exact test-bit mask is noted as TBD in the source.
