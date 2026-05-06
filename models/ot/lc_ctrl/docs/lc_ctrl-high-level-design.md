# LC_CTRL SystemC TLM High-Level Design Specification

**Document Version:** 1.0
**Date:** 2026-04-27
**IP Module:** Life-Cycle Controller (lc_ctrl)
**Modeling Approach:** SystemC TLM2.0, purely combinational register model

---

## Executive Summary

This document specifies the SystemC Transaction-Level Model (TLM) for the SEP Life-Cycle Controller (`lc_ctrl`). The model implements the **combinational feature-control logic** that translates fuse-burned life-cycle state, feature disable vectors, and runtime demotion states into a 64-bit `FEAT_CTRL` bitmap.

### Key Architectural Characteristics

- **No state machine** — the model has no time-varying internal state machine. `FEAT_CTRL` is recomputed combinationally whenever `DEMOTE_1`, `DEMOTE_2`, or any CCI parameter changes.
- **Purely parametric** — all fuse-burned inputs (`lc_state`, `sip_dis_*`, `sys_dis_*`, `security_disable`, `secure_tm`) are CCI parameters set via ini file. No recompilation is needed to test different life-cycle scenarios.
- **Live demotion** — `DEMOTE_1` and `DEMOTE_2` are writable W1S registers updated by BL1/BL2 firmware at runtime. `FEAT_CTRL` is recomputed immediately on every demotion write.
- **Single output** — the model exposes one 32-bit TLM target socket. All register interaction (FEAT_CTRL reads, DEMOTE writes) uses the same socket.

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Features](#2-features)
3. [Port Interfaces](#3-port-interfaces)
4. [Memory-Mapped Registers](#4-memory-mapped-registers)
5. [Register Callbacks](#5-register-callbacks)
6. [Internal Architecture](#6-internal-architecture)
7. [Modeling Assumptions](#7-modeling-assumptions)
8. [Conclusion](#8-conclusion)

---

## 1. Introduction

### 1.1 LC_CTRL Overview

The Life-Cycle Controller is the gatekeeper for all debug, test, and functional feature enables in the SEP chip. Its primary output, `FEAT_CTRL`, is a 64-bit bitmap consumed by other IP blocks to decide whether specific hardware features are enabled:

- **Bits [31:0]** (`DEBUG_BITS`) — debug and trace enable signals (sep_debug, soc_debug, ap_debug, ap_trace, sip_debug, …)
- **Bits [47:32]** (`TEST_BITS`) — test mode enables (fuse_test, sep_stest, sep_dtest, ap_stest, ap_dtest, …). Gated by `secure_tm`.
- **Bits [63:48]** (`FUNC_BITS`) — functional feature enables (func_reserved). Always honored in production states.

The value of `FEAT_CTRL` depends on:
1. The 4-bit life-cycle state burned into the chip (`lc_state`)
2. The SIP-level feature disable vector (`sip_dis`, 64 bits)
3. The system-level feature disable vector (`sys_dis`, 64 bits)
4. Whether either demotion domain has been activated (`DEMOTE_1`, `DEMOTE_2`)
5. Whether the security override flag is set (`security_disable`)
6. Whether the secure test-mode gate is active (`secure_tm`)

### 1.2 Purpose of This Document

This specification defines the SystemC TLM implementation of `lc_ctrl` for virtual platform integration. It documents:

- Features included and excluded from the TLM model
- External port interfaces for system-level integration
- Memory-mapped register specifications
- Register callback implementations and their side-effects
- The `compute_feat_ctrl()` algorithm and `get_demote_state()` accessor used by `keymgr_tt`

### 1.3 Modeling Goals

1. **Enable feature-gating simulation** — allow firmware to read `FEAT_CTRL` and observe correct debug/test/func enable state for any life-cycle configuration
2. **Support demotion flows** — model BL1/BL2 writing `DEMOTE_1`/`DEMOTE_2` and observing the resulting change in `FEAT_CTRL`
3. **Zero-recompile configuration** — all life-cycle inputs are CCI parameters; running different scenarios requires only changing the ini file
4. **Provide live demotion state for KM** — expose `get_demote_state()` so `keymgr_tt` can read current demotion at KDF invocation time

### 1.4 Target Audience

- SystemC model developers maintaining or extending `lc_ctrl`
- VP architects integrating `lc_ctrl` into the SEP subsystem
- SEP firmware engineers developing LC-aware driver code
- Verification engineers creating or extending the test plan

---

## 2. Features

### 2.1 Is (MUST Model)

#### 2.1.1 FEAT_CTRL Combinational Logic

The model faithfully mirrors the `always_comb` block in `sep_lifecycle_ctrl.sv`:

| LC State | Encoding | FEAT_CTRL Computation |
|----------|----------|-----------------------|
| TEST_DEV | 0x0 | `~(sip_dis \| sys_dis)` — all non-disabled features on |
| TEST_DEV + DEMOTE_1 or DEMOTE_2 | 0x0 + demote | `~(sip_dis \| sys_dis) \| DEBUG_BITS` — forces all debug bits on regardless of dis vectors |
| PROD (no demote) | 0x1 | `~(sip_dis \| sys_dis) & FUNC_BITS` — functional bits only; debug and test bits zeroed |
| PROD + DEMOTE_1 | 0x1 + demote1 | `~sip_dis & FUNC_BITS \| DEBUG_BITS` — SIP-relative func bits + full debug |
| PROD + DEMOTE_2 | 0x1 + demote2 | `~sys_dis & FUNC_BITS \| DEBUG_BITS` — SYS-relative func bits + full debug |
| PROD_END | 0x8 | `~(sip_dis \| sys_dis) & FUNC_BITS` — same as PROD base; no demote path |
| RMA_SIP | 0x2–0x3 (4'b001x) | `~sip_dis` — all SIP features except dis-masked ones |
| RMA_CHIPLET | 0x6–0x7 (4'b011x) | `0xFFFF_FFFF_FFFF_FFFF` — all features on |
| INVALID | all others | `0` — all features off |

After computing the state-dependent value, if `secure_tm == false`, `TEST_BITS [47:32]` are cleared from `FEAT_CTRL`.

If `security_disable == true`, the entire computation is short-circuited: `FEAT_CTRL = 0xFFFF_FFFF_FFFF_FFFF` regardless of any other input.

#### 2.1.2 DEMOTE Registers (W1S with Lock)

`DEMOTE_1` (domain-1, written by BL1 firmware) and `DEMOTE_2` (domain-2, written by BL2 firmware):

- **Bit 0** (`demote`) — Write-1-to-Set: writing 1 activates demotion for this domain; writing 0 has no effect
- **Bit 1** (`lock`) — Write-1-to-Set: once set, all subsequent writes to this register are silently ignored (prevents BL2 from undoing BL1 demote)
- Writing to a locked register generates a `CSML_REPORT(WARNING)` message

Each write to `DEMOTE_1` or `DEMOTE_2` triggers an immediate call to `compute_feat_ctrl()`, updating `FEAT_CTRL_LO`/`FEAT_CTRL_HI`.

#### 2.1.3 `get_demote_state()` Accessor

Public method used by `keymgr_tt` at KDF invocation time:

```
bits[1:0] = DEMOTE_1 & 0x1  (domain-1 demote bit)
bits[3:2] = DEMOTE_2 & 0x1  (domain-2 demote bit)
```

Returns a packed 4-bit value. Always 0 at `end_of_elaboration`; valid after BL1/BL2 firmware writes DEMOTE registers.

#### 2.1.4 CCI Parameter-Driven Configuration

All fuse-burned inputs are `csml_param` public members, configurable via ini file without recompilation:

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `lc_state` | `uint32_t` | 0 (TEST_DEV) | 4-bit life-cycle state encoding |
| `sip_dis_lo` | `uint32_t` | 0 | SIP feature disable bits [31:0] |
| `sip_dis_hi` | `uint32_t` | 0 | SIP feature disable bits [63:32] |
| `sys_dis_lo` | `uint32_t` | 0 | SYS feature disable bits [31:0] |
| `sys_dis_hi` | `uint32_t` | 0 | SYS feature disable bits [63:32] |
| `security_disable` | `bool` | false | When true: forces FEAT_CTRL = all-ones |
| `secure_tm` | `bool` | true | When false: clears TEST_BITS [47:32] from FEAT_CTRL |
| `verbosity` | `int` | 2 | CsmlLogger max verbosity |

#### 2.1.5 LC State Encoding Constants

Named `static constexpr` members expose the state encoding for use by other modules and tests:

```cpp
LC_STATE_TEST_DEV           = 0x0   // 4'b0000
LC_STATE_PROD               = 0x1   // 4'b0001
LC_STATE_PROD_END           = 0x8   // 4'b1000
LC_STATE_RANGE_MASK         = 0xE   // used for range matching
LC_STATE_RMA_SIP_BASE       = 0x2   // 4'b001x — matches 0x2, 0x3
LC_STATE_RMA_CHIPLET_BASE   = 0x6   // 4'b011x — matches 0x6, 0x7
// any value not above is INVALID
```

### 2.2 Is Not (EXCLUDE)

#### 2.2.1 Hardware LC State Transition Logic

- OTP programming and fuse blow control — lc_state is a fixed CCI parameter; no state machine progression
- Conditional token hashing (for RMA token transitions) — not modeled
- Alert/escalation outputs — no alert ports; `CSML_REPORT(WARNING)` used for lock violations

#### 2.2.2 Physical Interfaces

- Clock and reset ports — the model is a TLM component; `end_of_elaboration()` performs initialization, no SC_METHOD sensitive to `clk`
- OTP direct interface — no hardware OTP read/write bus; state received as CCI param
- Escalation input — not modeled

#### 2.2.3 Timing

- All register operations complete at `SC_ZERO_TIME`
- No cycle-accurate timing

---

## 3. Port Interfaces

| Category | Port Name | Type | Direction | Description |
|----------|-----------|------|-----------|-------------|
| **Register Bus** | `target_socket` (via `lc_ctrl_base`) | `tlm_utils::simple_target_socket<..., 32>` | Target | 32-bit TLM target for MMIO register access. Handles reads (FEAT_CTRL_LO/HI) and writes (DEMOTE_1, DEMOTE_2). Memory window: 0x18 bytes. |

### 3.1 Interface Notes

1. **No reset port** — there is no `rst_ni` input. `end_of_elaboration()` initialises `FEAT_CTRL` from CCI params; no runtime reset path is modeled.
2. **No clock port** — purely TLM; timing is transaction-based.
3. **No interrupt output** — `FEAT_CTRL` is read by firmware polling; no IRQ mechanism.
4. **`get_demote_state()` is a C++ method call**, not a TLM socket — consumed by `keymgr_tt` via a stored `std::function<uint32_t()>` callback set at `start_of_simulation`.

---

## 4. Memory-Mapped Registers

**Base address:** Assigned at VP integration time (not fixed in the model; bound to `target_socket` offset 0).

**Memory window size:** `0x18` bytes (4 registers × 4 bytes; DEMOTE_2 at 0x10 leaves a 4-byte gap at 0x0C).

| Register | Offset | Reset | Access | Description |
|----------|--------|-------|--------|-------------|
| **FEAT_CTRL_LO** | 0x0000 | 0x00000000 | RO | Lower 32 bits of 64-bit feature control. `read_mask=0xFFFFFFFF`, `write_mask=0x00000000`. Set by `compute_feat_ctrl()` at elaboration and on every DEMOTE write. |
| **FEAT_CTRL_HI** | 0x0004 | 0x00000000 | RO | Upper 32 bits of 64-bit feature control. `read_mask=0xFFFFFFFF`, `write_mask=0x00000000`. |
| **DEMOTE_1** | 0x0008 | 0x00000000 | RW W1S | Domain-1 demotion (BL1 firmware). `read_mask=0xFFFFFFFF`, `write_mask=0xFFFFFFFF`. W1S semantics enforced by write callback. Bit[0]=demote, Bit[1]=lock, Bits[31:2]=reserved. |
| **DEMOTE_2** | 0x0010 | 0x00000000 | RW W1S | Domain-2 demotion (BL2 firmware). `read_mask=0xFFFFFFFF`, `write_mask=0xFFFFFFFF`. Same bit layout as DEMOTE_1. |

### 4.1 FEAT_CTRL Bit Regions

| Bits | Region | Mask | Description |
|------|--------|------|-------------|
| [31:0] | `DEBUG_BITS` | `0x00000000_FFFFFFFF` | Debug and trace enables (sep_debug, soc_debug, ap_debug, ap_trace, sip_debug, debug_reserved) |
| [47:32] | `TEST_BITS` | `0x0000FFFF_00000000` | Test-mode enables (fuse_test, sep_stest, sep_dtest, ap_stest, ap_dtest, test_reserved). Gated by `secure_tm`. |
| [63:48] | `FUNC_BITS` | `0xFFFF0000_00000000` | Functional feature enables (func_reserved). Present in all production states. |

### 4.2 DEMOTE Register Bit Fields

| Bit | Field | Access | Description |
|-----|-------|--------|-------------|
| [0] | `demote` | W1S | Write 1 to activate demotion for this domain. Cleared only by cold reset. |
| [1] | `lock` | W1S | Write 1 to lock this register. Once set, all writes are ignored with a `CSML_REPORT(WARNING)`. |
| [31:2] | `reserved0` | — | Ignored on write, reads as 0. |

---

## 5. Register Callbacks

Both DEMOTE registers are registered with write callbacks in `lc_ctrl_model::lc_ctrl_model()` constructor.

### 5.1 `on_demote_1_write(DT value)` (DEMOTE_1 write)

1. Read current `DEMOTE_1` value
2. Check bit[1] (lock) — if set, emit `CSML_REPORT(WARNING, "DEMOTE_1 write ignored — lock bit is set")` and return `true` (no change)
3. If `value & 0x1` (demote bit): OR into current value — `current |= 0x1`
4. If `value & 0x2` (lock bit): OR into current value — `current |= 0x2`
5. Write updated value back to `DEMOTE_1`
6. Call `compute_feat_ctrl()` to recompute and update `FEAT_CTRL_LO`/`FEAT_CTRL_HI`
7. Return `true`

### 5.2 `on_demote_2_write(DT value)` (DEMOTE_2 write)

Identical logic to `on_demote_1_write` but operates on `DEMOTE_2`.

### 5.3 Callback Registration

Both callbacks are registered in the constructor (not `end_of_elaboration`), ensuring they are active from the moment TLM transactions are processed.

---

## 6. Internal Architecture

### 6.1 Class Hierarchy

```
lc_ctrl_model (sc_module, extends lc_ctrl_base)
├── lc_ctrl_base       (SCML register instances + target_socket)
│   ├── FEAT_CTRL_LO   @ 0x0000  RO
│   ├── FEAT_CTRL_HI   @ 0x0004  RO
│   ├── DEMOTE_1       @ 0x0008  W1S
│   └── DEMOTE_2       @ 0x0010  W1S
├── CsmlLogger         logger
├── csml_param<int>    verbosity
├── csml_param<uint32_t> lc_state, sip_dis_lo, sip_dis_hi, sys_dis_lo, sys_dis_hi
├── csml_param<bool>   security_disable, secure_tm
└── (private) void compute_feat_ctrl()
```

### 6.2 Initialization Flow

```
lc_ctrl_model constructor:
  → Initialize all csml_params with defaults
  → reset_all_registers() — set FEAT_CTRL=0, DEMOTE=0
  → register write callbacks for DEMOTE_1, DEMOTE_2

end_of_elaboration():
  → compute_feat_ctrl()  — applies ini-file params to produce initial FEAT_CTRL
```

### 6.3 `compute_feat_ctrl()` Algorithm

```
sip_dis = (sip_dis_hi << 32) | sip_dis_lo
sys_dis = (sys_dis_hi << 32) | sys_dis_lo
lc      = lc_state & 0xF
demote1 = (DEMOTE_1 & 0x1) != 0
demote2 = (DEMOTE_2 & 0x1) != 0

if security_disable:
    feat_ctrl = all-ones

elif lc == TEST_DEV:
    feat_ctrl = ~(sip_dis | sys_dis)
    if demote1 OR demote2:
        feat_ctrl |= DEBUG_BITS              // re-enable all debug bits

elif lc == PROD:
    feat_ctrl = ~(sip_dis | sys_dis) & FUNC_BITS   // base: func only
    if demote1:
        feat_ctrl = ~sip_dis & FUNC_BITS | DEBUG_BITS   // PROD_DBG_1
    elif demote2:
        feat_ctrl = ~sys_dis & FUNC_BITS | DEBUG_BITS   // PROD_DBG_2

elif lc == PROD_END:
    feat_ctrl = ~(sip_dis | sys_dis) & FUNC_BITS

elif (lc & RANGE_MASK) == RMA_SIP_BASE:   // 4'b001x
    feat_ctrl = ~sip_dis

elif (lc & RANGE_MASK) == RMA_CHIPLET_BASE: // 4'b011x
    feat_ctrl = all-ones

else:                                       // INVALID
    feat_ctrl = 0

if NOT secure_tm:
    feat_ctrl &= ~TEST_BITS               // gate test bits [47:32]

FEAT_CTRL_LO = feat_ctrl[31:0]
FEAT_CTRL_HI = feat_ctrl[63:32]
```

### 6.4 `get_demote_state()` Method

```cpp
uint32_t get_demote_state() const {
    uint8_t d1 = static_cast<uint32_t>(DEMOTE_1) & 0x1u;
    uint8_t d2 = static_cast<uint32_t>(DEMOTE_2) & 0x1u;
    return (d2 << 2) | d1;
}
```

Packed format: `bits[1:0]` = DEMOTE_1 value, `bits[3:2]` = DEMOTE_2 value. Used by `keymgr_tt` as one of the KDF inputs. Called at KDF invocation time (not at elaboration) because DEMOTE registers are written by firmware after boot.

---

## 7. Modeling Assumptions

### 7.1 Combinational Model

1. **No state machine** — the LC controller in RTL transitions through states over time; in the VP model, `lc_state` is a fixed CCI parameter representing the chip's fuse-burned state at power-on. State transitions during simulation are not modeled.

2. **FEAT_CTRL computed once at elaboration** — `end_of_elaboration()` sets the initial `FEAT_CTRL` from params. It is then recomputed only when DEMOTE registers are written. Changing CCI params after elaboration has no effect.

### 7.2 Demotion Semantics

3. **W1S demotion is one-way** — matching RTL: once `DEMOTE_1.demote=1`, it cannot be cleared except by cold reset (which is not modeled; simply not clearing it in the VP model is acceptable).

4. **Lock prevents re-demoting** — BL1 can lock its domain after demotion to prevent BL2 from toggling it. This is fully modeled.

5. **DEMOTE_2 independence** — the two demotion domains are completely independent. Both can be set simultaneously.

### 7.3 Feature Control Semantics

6. **TEST_DEV + demote ORs in DEBUG_BITS** — this re-enables debug features even if they were disabled via `sip_dis`/`sys_dis`. This is the RTL behavior: demotion overrides disable vectors for debug bits.

7. **`secure_tm` gates TEST_BITS regardless of LC state** — the test bit gate is applied last, after all LC-state logic, and applies to all states.

8. **INVALID state maps to 0** — any LC state encoding not matching the five defined categories (TEST_DEV, PROD, PROD_END, RMA_SIP, RMA_CHIPLET) produces `FEAT_CTRL=0`. This includes states 0x4, 0x5, 0x9–0xF.

### 7.4 CCI Parameters

9. **Params read once at `end_of_elaboration`** — `compute_feat_ctrl()` calls `get_param_value()` at the time it is called. If CCI params are modified after elaboration (not a supported flow), the change is not reflected.

10. **No inter-module wiring needed** — `lc_ctrl` does not accept live inputs from `sep_efuse` at runtime. `lc_state`, `sip_dis`, `sys_dis` are independent CCI params. In a production VP, these params should be set consistently with `sep_efuse`'s matching params.

---

## 8. Conclusion

### 8.1 Summary

The `lc_ctrl` SystemC TLM model implements a faithful replica of the combinational `FEAT_CTRL` computation from `sep_lifecycle_ctrl.sv`. Key characteristics:

1. **Four registers** — `FEAT_CTRL_LO`/`HI` (RO, computed) and `DEMOTE_1`/`DEMOTE_2` (W1S with lock).
2. **Purely parametric fuse state** — seven CCI parameters cover all life-cycle inputs; no recompilation needed for different scenarios.
3. **Live demotion** — DEMOTE writes trigger immediate `FEAT_CTRL` recomputation, matching the RTL's combinational path.
4. **`get_demote_state()` accessor** — exposes packed demotion state for `keymgr_tt` KDF input.
5. **All six LC state categories** implemented, including range-matched RMA states and the INVALID catch-all.

### 8.2 Known Gaps

| Gap | Risk | Mitigation |
|-----|------|------------|
| No LC state machine transitions | Low — VP use case only needs static state | Fixed CCI param covers all firmware development scenarios |
| No alert/escalation output | Low | Not needed for SEP SW integration |
| DEMOTE_2 at 0x0010 leaves gap at 0x000C | None — by design | Matches register map definition |
| sip_dis / sys_dis not cross-validated with sep_efuse params | Low | User must set consistent values in both models' ini files |

---

**End of High-Level Design Specification**
