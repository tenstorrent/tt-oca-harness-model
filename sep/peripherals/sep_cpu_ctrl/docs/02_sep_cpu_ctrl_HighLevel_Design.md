# SEP CPU Control / System CSR SystemC TLM High-Level Design Specification

**Document Version:** 1.1
**Date:** 2026-07-27
**IP Module:** SEP CPU Control / System CSR (`sep_cpu_ctrl`)
**Modeling Approach:** SystemC TLM2.0, mixed plain-storage + behavioural-callback register model

---

## Executive Summary

This document specifies the SystemC Transaction-Level Model (TLM) for `sep_cpu_ctrl` — SEP's consolidated system CSR block. Real hardware (`sep_system_csr.sv`) is the single register file backing not just CPU-level configuration but also the alias-remap/output-remap/filter CSR sub-blocks, `nmi_vec_o`, and the SMC↔SEP fuse-sense handshake; this VP model implements the CPU-control subset directly (32 registers at `0x00–0x1000`) while the alias-remap/output-remap/filter CSRs are modeled as their own separate peripherals (`local_master_alias_remap_ctrl`, `sep_output_remap_ctrl`, `sep_filter_ctrl`) rather than folded into this one class — a deliberate VP-side decomposition of what real hardware implements as one monolithic register block.

> **Revision 1.1 note:** a register-map audit against `docs/new_rtls/sep_cpu_ctrl.rdl` (the current RTL revision) found this model still carried three registers removed from real hardware in a later RTL update (`SPACC_CTRL`, `TIMEOUT_COUNT_TROOT`, `TIMEOUT_COUNT_SPACC`) plus matching stray bitfields (`spacc_cg_enable`, `troot_timeout_int`/`spacc_timeout_int`, `fast_spacc_en`), which had silently shifted every `TIMEOUT_COUNT_*`/`TIMEOUT_ENABLE`/`TIMEOUT_CLEAR`/`TIMEOUT_MODE` register to the wrong address. The audit also found `REFERENCE_COUNTER` had no write support at all despite being `sw=rw` in the RDL, which is what `sep_reference_counter_test` was actually failing on. All of this is fixed as of this revision; see §7.6 for the full account.

### Key Architectural Characteristics

- **This is the peripheral behind almost every cross-peripheral address window used elsewhere in the VP** — `SEP_GLOBAL_BASE_ADDR`/`SEP_LOCAL_BASE_ADDR`/`SEP_REGION_SIZE` are read live by `local_alias_remap_adapter` and `smn_inbound_remap_adapter` (`vp/platform/sep/inc/adapters.h`); `SMU_GLOBAL_BASE_ADDR`/`SMU_REGION_SIZE` define the SMU window merged into `outbound_filter_mux`. Getting this peripheral's register semantics right has direct, immediate consequences for three other subsystems documented elsewhere in this session's work.
- **Mixed storage model** — most registers are plain CSML read/write storage with no behavioural tie to anything; a handful (`SEP_NMI_VEC`, `EXT_TRNG_SRC_SEL`, `TIMEOUT_CLEAR`, `TIMEOUT_INTERRUPT`, `SEP_TEST_CTRL`, `SMC_FUSE_SENSE_STATUS`, `SEP_FUSE_SENSE_STATUS`, `SEP_STRAPS`, `SEP_VERSION_ID`, `REFERENCE_COUNTER`) have custom read and/or write callbacks that route through `hwif_in` (hardware-driven inputs) or lock-gated private shadow state.
- **CCI-parameter-driven hardware inputs** — nine straps/fuse-sense booleans (`smc_fuse_sense_done`, `sep_fuse_sense_done`, `sep_standalone`, `fast_*_en`, `test_en`, `bypass_mem_repair`) are `csml_param`s snapshotted into `hwif_in` once at `end_of_elaboration()`, mirroring how real straps/fuse values are latched at reset rather than polled continuously.
- **No hardware timeout counting** — `TIMEOUT_COUNT_*` (8 registers), `TIMEOUT_ENABLE`, and `TIMEOUT_MODE` are plain storage with no counting/expiry logic; only `TIMEOUT_INTERRUPT` (read) and `TIMEOUT_CLEAR` (write) are behaviourally wired, and only to the `hwif_in.*_timeout_int` flags — nothing in this VP currently sets those flags to begin with (see §2.2.1).
- **`KM_WIPE_CTRL` is new in this RTL revision** — a plain `wipe_state` bit with no downstream Key Manager wipe consumer in this VP (there is no Key Manager wipe pathway modeled at all yet); it is register-map-correct but behaviourally dormant, the same posture as the timeout-counting infrastructure (see §2.1.9).

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

### 1.1 SEP CPU Control Overview

`sep_cpu_ctrl` hosts the CSR fields SEP firmware uses for:

- **Clock/power gating configuration** (`CLOCK_GATE_CTRL`) — per-block enable bits, informational only in this VP.
- **Free-running-style diagnostic counter** (`REFERENCE_COUNTER`).
- **Bus/fabric timeout infrastructure** (`TIMEOUT_INTERRUPT`, `TIMEOUT_COUNT_*` ×8, `TIMEOUT_ENABLE`, `TIMEOUT_CLEAR`, `TIMEOUT_MODE`) — one set of fields per monitored path (DMA, SYS_IN, alias-remap, filter-out, entropy read/write, mailbox in/out).
- **DPA-countermeasure controls** (`PKA_CTRL`) — noise-source enable/valid bits for the public-key accelerator's differential-power-analysis defenses. (The symmetric-crypto/SPACC equivalent, `SPACC_CTRL`, existed in an earlier RTL revision and has been removed — see the revision note above.)
- **Boot-critical address windows** (`SEP_GLOBAL_BASE_ADDR`, `SEP_LOCAL_BASE_ADDR`, `SEP_REGION_SIZE`, `SMU_GLOBAL_BASE_ADDR`, `SMU_REGION_SIZE`) — consumed live by the local-alias and SMN-inbound remap adapters and by the outbound SMU merge path.
- **SMC↔SEP fuse-sense handshake** (`SMC_FUSE_SENSE_STATUS`, `SEP_FUSE_SENSE_STATUS`) — gates SEP's own fuse operations on SMC completing its fuse sense first (per the OCAH boot sequence).
- **Boot straps readback** (`SEP_STRAPS`, `SEP_TEST_CTRL`).
- **NMI vector and external TRNG source select**, each with a firmware-settable, one-way lock bit (`SEP_NMI_VEC`/`SEP_NMI_VEC_LOCK`, `EXT_TRNG_SRC_SEL`/`EXT_TRNG_SRC_SEL_LOCK`).
- **Key Manager emergency wipe control** (`KM_WIPE_CTRL`) — a software-set `wipe_state` bit; new in this RTL revision, register-map-correct but with no downstream consumer in this VP (§2.1.9).
- **Identification/debug** (`SEP_VERSION_ID`, `SEP_SW_DEBUG`, `RAS_BANK_INFO`).

### 1.2 Purpose of This Document

This high-level design specification defines the SystemC TLM implementation for VP integration. It documents:

- Which of the 32 registers have real behavioural logic vs. plain storage
- The CCI-parameter hardware-input mechanism and its snapshot timing
- The lock-bit (WOSET-like) pattern shared by `SEP_NMI_VEC` and `EXT_TRNG_SRC_SEL`
- The `TIMEOUT_CLEAR`/`TIMEOUT_MODE` write-mask fix (previously both were `0x0`, silently blocking all firmware writes)
- Cross-references to every other VP component that depends on this peripheral's register values

### 1.3 Modeling Goals

1. **Correct address-window delivery** — `SEP_GLOBAL_BASE_ADDR`/`SEP_LOCAL_BASE_ADDR`/`SEP_REGION_SIZE`/`SMU_GLOBAL_BASE_ADDR`/`SMU_REGION_SIZE` must be readable live (not just at elaboration) by any adapter that depends on them, since firmware can reprogram these at boot.
2. **Correct NMI vector delivery** — `nmi_vec_o` must reflect `SEP_NMI_VEC` immediately on write (subject to the lock), matching real hardware's direct wire-out behavior, not just CSR readback.
3. **Zero-recompile strap/fuse-sense scenario testing** — all ten `hwif_in` boolean inputs are CCI parameters; VP test scenarios select strap combinations via ini file, not recompilation.
4. **Faithful lock-bit semantics** — once `SEP_NMI_VEC_LOCK`/`EXT_TRNG_SRC_SEL_LOCK` is set, the corresponding value register becomes permanently read-only until reset, with no bypass.

### 1.4 Target Audience

- SystemC model developers maintaining or extending `sep_cpu_ctrl`
- VP architects and developers of any component reading its live registers (`local_alias_remap_adapter`, `smn_inbound_remap_adapter`, `outbound_filter_mux`'s SMU leg, `stdout_device`'s NMI-vector write path)
- SEP firmware engineers configuring boot-time address windows, timeout thresholds, or TRNG source selection
- Verification engineers creating or extending the test plan, especially around the `TIMEOUT_CLEAR` fix and the fuse-sense CCI parameters

---

## 2. Features

### 2.1 Is (MUST Model)

#### 2.1.1 Boot-Critical Address Windows — Live, Not Snapshotted

`SEP_GLOBAL_BASE_ADDR` (0xC0, reset 0x0), `SEP_LOCAL_BASE_ADDR` (0xC8, reset **0xC0000000**), `SEP_REGION_SIZE` (0xD0, reset **0x01000000**), `SMU_GLOBAL_BASE_ADDR` (0x100, reset **0x80000000**), `SMU_REGION_SIZE` (0x110, reset **0x40000000**) are plain CSML storage — no callback — but critically that means every read by an external C++ consumer (`static_cast<uint64_t>(cpu_ctrl->SEP_LOCAL_BASE_ADDR)`, etc.) sees the *current* value, including any firmware write that happened moments earlier. This is what lets `local_alias_remap_adapter` and `smn_inbound_remap_adapter` react to firmware reprogramming these windows without any explicit notification mechanism — they just re-read the register on every transaction.

**Consumers, for cross-reference:**
- `SEP_LOCAL_BASE_ADDR` + `SEP_REGION_SIZE` → `local_alias_remap_adapter` (fixed local-alias remap, CPU/DMA ingress path)
- `SEP_GLOBAL_BASE_ADDR` + `SEP_REGION_SIZE` (shared) → `smn_inbound_remap_adapter` (SMN-inbound global→local remap)
- `SMU_GLOBAL_BASE_ADDR` + `SMU_REGION_SIZE` → the SMU window's bus `PortMapping`, forwarding into `outbound_filter_mux`'s `smu_tgt`

#### 2.1.2 NMI Vector with Lock (`SEP_NMI_VEC` / `SEP_NMI_VEC_LOCK`)

`SEP_NMI_VEC` (0x180, reset `0x60000080`) has both custom read and write callbacks:

```cpp
handle_write_SEP_NMI_VEC(value):
    if (!fs_nmi_vec_lock_) {
        fs_nmi_vec_ = (value >> 1) & 0x7FFFFFFF
        nmi_vec_o.write(fs_nmi_vec_ << 1)   // drives the physical output port immediately
    }

handle_read_SEP_NMI_VEC():
    return fs_nmi_vec_ << 1
```

`SEP_NMI_VEC_LOCK` (0x188) is a one-way (WOSET-like) lock: `fs_nmi_vec_lock_ |= bool(value & write_bit_mask)` — once set by any write with a nonzero masked value, it can never be cleared again (no reset-independent unlock path other than a full peripheral reset). Once locked, further writes to `SEP_NMI_VEC` are silently ignored.

`nmi_vec_o` is a real `sc_out<uint32_t>` port — this is not just CSR bookkeeping, it's a live signal other VP components observe. `stdout_device` also writes this same downstream `nmi_vec_signal` via its own `LOAD_NMI_ADDR` mechanism (see `och_sep_ss.hpp`'s comment: *"NMI address — written by stdout_device (LOAD_NMI_ADDR) or sep_cpu_ctrl"*) — both paths drive the same wire, so whichever writes last wins, matching how a shared signal works in real hardware.

#### 2.1.3 External TRNG Source Select with Lock (`EXT_TRNG_SRC_SEL` / `EXT_TRNG_SRC_SEL_LOCK`)

Same lock pattern as §2.1.2, applied to a 2-bit `sel` field (reset `0x3`): `handle_write_EXT_TRNG_SRC_SEL` only updates `fs_ext_trng_src_sel_` while `!fs_ext_trng_src_sel_lock_`; `EXT_TRNG_SRC_SEL_LOCK` is WOSET. This selects which entropy source feeds the TRNG distribution path (consumed downstream by `entropy_src`/`edn` peripherals via `ext_trng_src_sel_o` in real hardware).

#### 2.1.4 `TIMEOUT_CLEAR` — Singlepulse, Now Correctly Writable

```cpp
handle_write_TIMEOUT_CLEAR(value):
    for each of the 10 timeout-interrupt bits:
        if (value bit set) hwif_in.<source>_timeout_int = false
    TIMEOUT_CLEAR = 0   // self-clears; never retains state (sw=w, no storage)
```

This register's `write_bit_mask` is `0x3ff` (all 10 clear bits genuinely writable) — **previously this mask was `0x0`, silently discarding every firmware write to this register with no error indication.** `TIMEOUT_MODE`'s mask was fixed the same way (now `0xfffff`, was `0x0`); unlike `TIMEOUT_CLEAR`, `TIMEOUT_MODE` has no callback — it's now plain writable storage, matching the intent that firmware selects per-source timeout behavior (e.g. one-shot vs. periodic) without that selection being silently dropped.

#### 2.1.5 Live-Computed Read Registers

Four registers have read callbacks that compute their value from `hwif_in` on every access rather than trusting stored CSML state:

- `TIMEOUT_INTERRUPT` (0x18) — ORs together the 10 `hwif_in.*_timeout_int` flags.
- `SEP_TEST_CTRL` (0xB0) — ORs together the 7 `hwif_in.fast_*_en`/`sep_standalone` flags (all CCI-parameter-sourced).
- `SMC_FUSE_SENSE_STATUS` (0x140) / `SEP_FUSE_SENSE_STATUS` (0x150) — each a single bit from `hwif_in`.
- `SEP_STRAPS` (0x160) — `test_en` (bit 0) and `bypass_mem_repair` (bit 1), both CCI-parameter-sourced.

#### 2.1.6 `REFERENCE_COUNTER` — Increment-on-Read, Writable/Reloadable

```cpp
handle_read_REFERENCE_COUNTER(value):  value = ++hwif_in.reference_counter_rc;
handle_write_REFERENCE_COUNTER(value): hwif_in.reference_counter_rc = value & write_bit_mask;
```

Every CSR read increments and returns a monotonically increasing 64-bit counter. A software write reloads it to the written value — matching real hardware's `wr_swacc` path (`prim_refclk_count_w_cdc`'s `i_cnt_update`/`i_cnt_update_value` in `sep_system_csr.sv`, which reprograms the free-running reference-clock counter on a CSR write and lets it resume counting from there) — after which subsequent reads keep incrementing from the new value. See §7.1 for why the *rate* of advance (per-read, not per-elapsed-time) is still an approximation, and §7.6 for why the write path didn't exist until this revision.

#### 2.1.7 CCI-Parameter Hardware Inputs, Snapshotted Once

Nine `csml_param`s (`smc_fuse_sense_done`, `sep_fuse_sense_done`, `sep_standalone`, `fast_spi_en`, `fast_iccm_en`, `fast_dccm_en`, `fast_sram_en`, `fast_pka_en`, `test_en`, `bypass_mem_repair`) are copied into `hwif_in` exactly once, in `end_of_elaboration()` — after `load_config_file()` has processed the ini file but before `sc_start()`, so all values are stable and firmware-visible from the very first CSR read.

#### 2.1.8 Reset

`SC_METHOD` sensitive to any edge of `rst_ni`, active when low: calls `reset_all_registers()`, zeroes `hwif_in.reference_counter_rc` (the free-running counter restarts at 0 on cold reset per the RDL's `REFERENCE_COUNTER` description — this is state outside CSML storage, same reasoning as the private-shadow fields below), then re-applies the three private-shadow defaults (`fs_nmi_vec_=0x60000080`, `fs_nmi_vec_lock_=false`, `fs_ext_trng_src_sel_=0x3`, `fs_ext_trng_src_sel_lock_=false`) and re-drives `nmi_vec_o` — necessary because those fields live in private C++ state, not CSML storage, and would otherwise survive a reset untouched.

#### 2.1.9 `KM_WIPE_CTRL` — Plain Register, No Wipe Consumer

`KM_WIPE_CTRL` (0x1A0, reset 0x0) is new in this RTL revision — a single `wipe_state` bit, `sw=rw`/`hw=r` in the RDL, meaning software sets it and hardware (the Key Manager) is expected to observe a rising edge and zero all KPV entries. This VP models it as plain read/write CSML storage with no callback, matching the pattern already used for other simple software-owned registers (`SEP_SW_DEBUG`, `RAS_BANK_INFO`) — there is no Key Manager wipe pathway modeled anywhere in this VP to actually react to it. The register is address-map-correct (software can set/clear/read the bit) but behaviourally inert, the same posture as the dormant timeout-counting infrastructure in §2.2.1.

### 2.2 Is Not (EXCLUDE)

#### 2.2.1 Hardware Timeout Counting

`TIMEOUT_COUNT_DMA/SYS_IN/MAILBOX_INBOUND/MAILBOX_OUTBOUND/ENTROPY_WRITE/ENTROPY_READ/FILTER_OUT/ALIAS_REMAP` (8 distinct 48-bit counters, all reset 0, all plain RW storage) and `TIMEOUT_ENABLE` (reset 0, mask `0xff`) have **no counting or expiry logic at all** — nothing in this VP increments them, compares them against a threshold, or sets the corresponding `hwif_in.*_timeout_int` flag as a consequence of elapsed time or a stalled transaction. The only way any `TIMEOUT_INTERRUPT` bit becomes visibly set today is if some *other* piece of test/VP code directly pokes `hwif_in` — which nothing currently does. `TIMEOUT_CLEAR`'s callback logic is real and correct, but has nothing to clear in practice.

#### 2.2.2 Clock/Power Gating Effects

`CLOCK_GATE_CTRL` (0x08, reset `0x1f0083`, mask `0x3f1fdf`) is plain storage — the eleven per-block clock-gate-enable bits and the `cg_hysteresis` field are readable/writable by firmware but have zero effect on any other peripheral's behavior or simulated timing. No power/clock-gating simulation exists in this VP.

#### 2.2.3 DPA Countermeasure Effects

`PKA_CTRL` (0x20, reset `0x1`, mask `0x10101`) stores `pka_dpa_disable`, `pka_noise_src`, `pka_noise_src_valid` fields for firmware to configure differential-power-analysis countermeasures on the PKA accelerator. No actual DPA noise injection or side-channel modeling exists in this VP; these are inert configuration bits. (The equivalent `SPACC_CTRL` register existed in an earlier RTL revision and has been removed from real hardware — see the revision note in the Executive Summary.)

#### 2.2.4 RAS Bank Identification Effects

`RAS_BANK_INFO` (0x170, mask `0xff`) stores `bank_chip`/`bank_instance` fields with no consumer anywhere in the VP — purely a readback value for firmware, informational only in a single-chiplet standalone VP.

#### 2.2.5 `SEP_SW_DEBUG`

A generic 32-bit RW scratch register (0x178) with no special semantics — a plain firmware-to-firmware (or firmware-to-debugger) scratch word, not tied to any behavior.

#### 2.2.6 `SEP_VERSION_ID`

Fixed constant `0xDEADBEEF` (0x1000) — both the register's own reset value *and* its read callback return the same literal; there is no build-time version-string mechanism to model.

#### 2.2.7 Timing

All register operations complete synchronously within `b_transport()`; there is no `sc_time` delay modeling the real fabric's CSR access latency.

---

## 3. Port Interfaces

| Category | Port/Socket Name | Type | Direction | Description |
|----------|-------------------|------|-----------|--------------|
| **Register Bus** | `target_socket` (via `sep_cpu_ctrl_base`) | `tlm_utils::simple_target_socket<csml_memory<64>, 32>` | Target | 64-bit TLM target. Memory window: `0x1008` bytes (registers span `0x00`–`0x1A0`, plus `SEP_VERSION_ID` at `0x1000`). |
| **Reset** | `rst_ni` | `sc_in<bool>` | Input | Active on any edge (see §7.2); acts when low. |
| **NMI Vector Output** | `nmi_vec_o` | `sc_out<uint32_t>` | Output | Driven by `SEP_NMI_VEC` writes (subject to lock) and on reset. Shared downstream signal, also driven by `stdout_device`. |
| **Hardware Inputs** | `hwif_in` (struct member) | C++ struct, not a port | — | `SepCpuCtrlHwifIn`: 9 boot-time CCI-sourced booleans + 8 timeout-interrupt booleans + a free-running counter seed. Populated from CCI params at `end_of_elaboration()`; the 8 timeout-interrupt flags have no other writer in this VP (§2.2.1). |

### 3.1 Interface Notes

1. **Physical base address** (`0x10A3_0000` per `Args.hpp`'s `cpu_ctrl_start_addr`, matching `SEP_CPU_CTRL_REG_MAP_BASE_ADDR`) is a VP integration detail, not encoded in the model itself.
2. **`nmi_vec_o` must be bound** — like every other real output port in this VP, elaboration fails if nothing is connected to it (the same class of issue documented for `smn_inbound_socket` in `Abstractions.md`/session history, though this specific port has always been bound correctly in `och_sep_ss.hpp` and the `smn_inbound_testbench.cpp` unit test).
3. **`hwif_in`'s eight timeout-interrupt flags are public and directly settable** — a test or future VP component wanting to exercise `TIMEOUT_INTERRUPT`/`TIMEOUT_CLEAR` can simply write `cpu_ctrl->hwif_in.sys_in_timeout_int = true;` directly; there is no encapsulation preventing this backdoor.

---

## 4. Memory-Mapped Registers

32 registers spanning `0x000`–`0x1A0` plus `SEP_VERSION_ID` at `0x1000`, all 64-bit CSML registers (`csml_reg<64>`) even where the real field is narrower.

| Register | Offset | Reset | Mask | Access | Description |
|----------|--------|-------|------|--------|-------------|
| **CLOCK_GATE_CTRL** | 0x008 | 0x1F0021 | 0x3F07FF | RW (plain) | 11 per-block clock-gate-enable bits + `cg_hysteresis`. No functional effect (§2.2.2). |
| **REFERENCE_COUNTER** | 0x010 | 0x0 | 0xFFFFFFFFFFFFFFFF | RW (callback) | Increments and returns on every read; software write reloads it (§2.1.6). |
| **TIMEOUT_INTERRUPT** | 0x018 | 0x0 | — | RO (callback) | 8 live bits from `hwif_in.*_timeout_int` (§2.1.5). |
| **PKA_CTRL** | 0x020 | 0x1 | 0x10101 | RW (plain) | PKA DPA-countermeasure config (§2.2.3). |
| **TIMEOUT_COUNT_DMA** | 0x028 | 0x0 | 0xFFFFFFFFFFFF | RW (plain) | 48-bit counter, no counting logic (§2.2.1). |
| **TIMEOUT_COUNT_SYS_IN** | 0x030 | 0x0 | 0xFFFFFFFFFFFF | RW (plain) | Same. |
| **TIMEOUT_COUNT_MAILBOX_INBOUND** | 0x038 | 0x0 | 0xFFFFFFFFFFFF | RW (plain) | Same. |
| **TIMEOUT_COUNT_MAILBOX_OUTBOUND** | 0x040 | 0x0 | 0xFFFFFFFFFFFF | RW (plain) | Same. |
| **TIMEOUT_COUNT_ENTROPY_WRITE** | 0x048 | 0x0 | 0xFFFFFFFFFFFF | RW (plain) | Same. |
| **TIMEOUT_COUNT_ENTROPY_READ** | 0x050 | 0x0 | 0xFFFFFFFFFFFF | RW (plain) | Same. |
| **TIMEOUT_COUNT_FILTER_OUT** | 0x058 | 0x0 | 0xFFFFFFFFFFFF | RW (plain) | Same. |
| **TIMEOUT_COUNT_ALIAS_REMAP** | 0x060 | 0x0 | 0xFFFFFFFFFFFF | RW (plain) | Same. |
| **TIMEOUT_ENABLE** | 0x068 | 0x0 | 0xFF | RW (plain) | 8 per-source timeout-enable bits, no gating effect (§2.2.1). |
| **TIMEOUT_CLEAR** | 0x070 | 0x0 | 0xFF | WO (callback, singlepulse) | Clears `hwif_in.*_timeout_int` bits; self-clears (§2.1.4). |
| **TIMEOUT_MODE** | 0x078 | 0x0 | 0xFFFF | RW (plain) | 8 × 2-bit per-source mode fields; writable but inert (§2.1.4). |
| **SEP_TEST_CTRL** | 0x0B0 | 0x0 | — | RO (callback) | 6 live bits from `hwif_in` (§2.1.5). |
| **SEP_GLOBAL_BASE_ADDR** | 0x0C0 | 0x0 | 56-bit | RW (plain) | Read live by `smn_inbound_remap_adapter` (§2.1.1). |
| **SEP_LOCAL_BASE_ADDR** | 0x0C8 | **0xC0000000** | 56-bit | RW (plain) | Read live by `local_alias_remap_adapter` (§2.1.1). |
| **SEP_REGION_SIZE** | 0x0D0 | **0x01000000** | 32-bit | RW (plain) | Shared by both adapters above (§2.1.1). |
| **SMU_GLOBAL_BASE_ADDR** | 0x100 | **0x80000000** | 56-bit | RW (plain) | Defines the SMU window merged into `outbound_filter_mux` (§2.1.1). |
| **SMU_REGION_SIZE** | 0x110 | **0x40000000** | 32-bit | RW (plain) | Same. |
| **SMC_FUSE_SENSE_STATUS** | 0x140 | 0x0 | — | RO (callback) | `hwif_in.smc_fuse_sense_done` (§2.1.5; currently hardcoded `true` in `och_sep_ss.hpp` — see `Abstractions.md` §3.5). |
| **SEP_FUSE_SENSE_STATUS** | 0x150 | 0x0 | — | RO (callback) | `hwif_in.sep_fuse_sense_done` (same caveat). |
| **SEP_STRAPS** | 0x160 | 0x0 | — | RO (callback) | `test_en`, `bypass_mem_repair` (§2.1.5). |
| **RAS_BANK_INFO** | 0x170 | 0x0 | 0xFF | RW (plain) | `bank_chip`/`bank_instance`, no consumer (§2.2.4). |
| **SEP_SW_DEBUG** | 0x178 | 0x0 | 0xFFFFFFFF | RW (plain) | Generic scratch (§2.2.5). |
| **SEP_NMI_VEC** | 0x180 | **0x60000080** | 0xFFFFFFFE | RW (callback, lock-gated) | Drives `nmi_vec_o` (§2.1.2). |
| **SEP_NMI_VEC_LOCK** | 0x188 | 0x0 | 0x1 | RW (callback, WOSET) | Locks `SEP_NMI_VEC` permanently once set. |
| **EXT_TRNG_SRC_SEL** | 0x190 | **0x3** | 0x3 | RW (callback, lock-gated) | 2-bit TRNG source select (§2.1.3). |
| **EXT_TRNG_SRC_SEL_LOCK** | 0x198 | 0x0 | 0x1 | RW (callback, WOSET) | Locks `EXT_TRNG_SRC_SEL` permanently once set. |
| **KM_WIPE_CTRL** | 0x1A0 | 0x0 | 0x1 | RW (plain) | `wipe_state` bit; no downstream KM wipe consumer (§2.1.9). |
| **SEP_VERSION_ID** | 0x1000 | **0xDEADBEEF** | — | RO (callback + matching reset) | Fixed constant (§2.2.6). |

---

## 5. Register Callbacks

### 5.1 Lock-Gated Write Pattern (`SEP_NMI_VEC`, `EXT_TRNG_SRC_SEL`)

```cpp
bool handle_write_X(DT value, DT write_bit_mask) {
    if (!fs_x_lock_)
        fs_x_ = extract(value, write_bit_mask);   // update private shadow
    return true_or_false;                          // NMI path also drives nmi_vec_o here
}
bool handle_write_X_LOCK(DT value, DT write_bit_mask) {
    fs_x_lock_ |= bool(value & write_bit_mask);     // WOSET — never cleared except by reset
    return false;   // lock register itself has no CSML-visible storage effect
}
```

Both `X_LOCK` write callbacks return `false` — the underlying `handle_write` default (which would otherwise store the raw value into the CSML word) never runs; the lock bit's CSML-visible state comes entirely from the paired read callback returning `fs_x_lock_` instead of any stored word.

### 5.2 `TIMEOUT_CLEAR` — Write-Only Side Effect, No Storage

Unlike the lock pattern above, `TIMEOUT_CLEAR` returns `true` after zeroing itself back out (`TIMEOUT_CLEAR = 0`) — the register briefly "holds" the written value only long enough for the callback to inspect which bits were set, then immediately self-clears, matching real hardware's `singlepulse` semantics (a register that generates a one-cycle pulse per set bit and never retains state).

### 5.3 Callback Registration

17 callbacks total (6 write: `SEP_NMI_VEC`, `SEP_NMI_VEC_LOCK`, `EXT_TRNG_SRC_SEL`, `EXT_TRNG_SRC_SEL_LOCK`, `TIMEOUT_CLEAR`, `REFERENCE_COUNTER`; 11 read: those five plus `TIMEOUT_INTERRUPT`, `SEP_TEST_CTRL`, `SMC_FUSE_SENSE_STATUS`, `SEP_FUSE_SENSE_STATUS`, `SEP_STRAPS`, `SEP_VERSION_ID`) are registered in `register_callbacks()`, called once from the constructor, using `std::bind` against `memory.register_write_callback`/`register_read_callback` at each register's fixed `offset`. There is no per-instance/array pattern here (unlike `sep_filter_ctrl`'s per-entry callback registration loop) since `sep_cpu_ctrl` has no repeated register groups.

---

## 6. Internal Architecture

### 6.1 Class Hierarchy

```
sep_cpu_ctrl_ip (sc_module, extends sep_cpu_ctrl_base)
├── sep_cpu_ctrl_base                (CSML register instances + target_socket)
│   ├── CLOCK_GATE_CTRL               @ 0x008
│   ├── REFERENCE_COUNTER             @ 0x010
│   ├── TIMEOUT_INTERRUPT             @ 0x018
│   ├── PKA_CTRL                       @ 0x020
│   ├── TIMEOUT_COUNT_* [8]           @ 0x028–0x060
│   ├── TIMEOUT_ENABLE/CLEAR/MODE     @ 0x068–0x078
│   ├── SEP_TEST_CTRL                 @ 0x0B0
│   ├── SEP_GLOBAL/LOCAL_BASE_ADDR, SEP_REGION_SIZE   @ 0x0C0–0x0D0
│   ├── SMU_GLOBAL_BASE_ADDR, SMU_REGION_SIZE          @ 0x100–0x110
│   ├── SMC/SEP_FUSE_SENSE_STATUS     @ 0x140–0x150
│   ├── SEP_STRAPS                    @ 0x160
│   ├── RAS_BANK_INFO, SEP_SW_DEBUG   @ 0x170–0x178
│   ├── SEP_NMI_VEC(_LOCK)            @ 0x180–0x188
│   ├── EXT_TRNG_SRC_SEL(_LOCK)       @ 0x190–0x198
│   ├── KM_WIPE_CTRL                  @ 0x1A0
│   └── SEP_VERSION_ID                @ 0x1000
├── rst_ni, nmi_vec_o                 (sc_in/sc_out)
├── SepCpuCtrlHwifIn hwif_in           (public struct — see §3)
├── csml_param<uint32_t> × 9          (boot-time straps/fuse-sense)
└── (private)
    ├── uint32_t fs_nmi_vec_               = 0x60000080
    ├── bool     fs_nmi_vec_lock_          = false
    ├── uint8_t  fs_ext_trng_src_sel_      = 0x3
    └── bool     fs_ext_trng_src_sel_lock_ = false
```

### 6.2 Initialization Flow

```
sep_cpu_ctrl_ip constructor:
  → sep_cpu_ctrl_base constructor: allocate csml_memory<64> (0x1008 bytes / 8 = 0x201 words),
    construct all 32 register objects
  → construct 9 csml_param<uint32_t> members (defaults 0)
  → SC_METHOD(reset_handler), sensitive to rst_ni (any edge)
  → register_callbacks()

end_of_elaboration():
  → snapshot all 9 csml_params into hwif_in (one-time, after ini file is loaded)

reset_handler() [whenever rst_ni reads low]:
  → reset_all_registers()   — CSML registers back to their declared reset values
  → zero hwif_in.reference_counter_rc (free-running counter restarts at 0)
  → re-apply the 4 private-shadow defaults (fs_nmi_vec_, fs_nmi_vec_lock_, etc.)
  → re-drive nmi_vec_o from the just-reset fs_nmi_vec_
```

### 6.3 Why Some Fields Live Outside CSML Storage

`fs_nmi_vec_`, `fs_nmi_vec_lock_`, `fs_ext_trng_src_sel_`, `fs_ext_trng_src_sel_lock_` are private `sep_cpu_ctrl_ip` members, not CSML register fields, specifically because their read-back value must be **independent of the write mask actually applied to the CSML word** — e.g. `SEP_NMI_VEC`'s write callback shifts and re-masks the incoming value into a 31-bit shadow before ever deciding whether to honor it (lock check), and the read callback reconstructs the CSR-visible value from that shadow rather than from whatever the default `handle_write` would have stored. This lets the lock/shift logic be expressed once in C++ rather than through CSML's declarative field/mask mechanism, which has no native concept of "conditionally reject this write."

---

## 7. Modeling Assumptions

### 7.1 `REFERENCE_COUNTER`'s Rate of Advance Is Still Increment-on-Read, Not Free-Running

Real hardware's reference counter (`prim_refclk_count_w_cdc` in `sep_system_csr.sv`) increments every `clk_ref_i` cycle regardless of software access, and a software write reprograms it (via a CDC-synced update path) after which it resumes counting from the new value. This model gets the *write/reload* half right as of §2.1.6/§7.6, but still only advances the value when firmware actually reads the register (`++hwif_in.reference_counter_rc` happens inside the read callback) rather than on a clock-driven cadence. Consequences:

- Two reads separated by a large amount of *simulated* wall-clock time (but no intervening reads) will differ by exactly 1, not by an amount proportional to elapsed cycles — firmware that uses this register to measure elapsed *time* (rather than just "has any read happened since I last checked, and does a write take effect") will get incorrect measurements.
- This is adequate for firmware that needs a monotonically-increasing, always-distinct, reloadable value — which is exactly what `sep_reference_counter_test.c` exercises (poll-until-advance, write-and-verify-reload, then poll-until-advance again) — but would need to become a real `SC_METHOD` sensitive to a clock signal (or a periodic `sc_spawn`) if a test ever depends on genuine elapsed-time semantics.

### 7.2 Reset Sensitivity

Same note as `local_master_alias_remap_ctrl`: `reset_handler()` is sensitive to `rst_ni` on any edge, not `.neg()` only, but the handler's own `if (!rst_ni.read())` guard makes this observably identical to negedge-only sensitivity — a positive edge simply re-enters the method and does nothing.

### 7.3 Timeout Infrastructure Is Present but Dormant

The complete CSR surface for the timeout subsystem (10 sources × count/enable/clear/mode) exists and is correctly writable end-to-end (including the write-mask fix documented in §2.1.4), but there is no VP-side producer of a timeout condition. A firmware test that deliberately stalls a monitored path (DMA, mailbox, alias-remap, etc.) and expects `TIMEOUT_INTERRUPT` to eventually assert will not see that happen in this VP — the only way to exercise `TIMEOUT_CLEAR`'s logic today is to directly poke `hwif_in.<source>_timeout_int = true` from test code (the backdoor noted in §3.1), not through any organic stall condition.

### 7.4 Fuse-Sense Handshake Is CCI-Configured, Currently Overridden to Always-True

`SMC_FUSE_SENSE_STATUS`/`SEP_FUSE_SENSE_STATUS` are correctly wired to `hwif_in.smc_fuse_sense_done`/`sep_fuse_sense_done`, which are themselves correctly CCI-parameter-driven inside this peripheral — but `och_sep_ss.hpp` currently hardcodes both `hwif_in` fields to `true` directly (`cpu_ctrl->hwif_in.smc_fuse_sense_done = true;`) at construction time, bypassing the CCI-param path entirely for the standalone VP. This is tracked as an integration-readiness gap in `Abstractions.md` §3.5 (real hardware requires SEP to wait for SMC's own fuse-sense signal, which has no live source in a SEP-only VP) — not a defect in this peripheral itself, which implements the correct mechanism; it's simply not being exercised as designed by the current top-level wiring.

### 7.5 Address-Window Registers Have No Programmability Bounds Checking

`SEP_LOCAL_BASE_ADDR`/`SEP_GLOBAL_BASE_ADDR`/`SMU_GLOBAL_BASE_ADDR` accept any 56-bit value, and `SEP_REGION_SIZE`/`SMU_REGION_SIZE` accept any 32-bit value, with no range validation. Firmware could program a `SEP_REGION_SIZE` of `0xFFFFFFFF` (4 GiB) and nothing in this model would reject it — real hardware's actual bounds (if any exist beyond the field width itself) are not enforced here; downstream consumers (`local_alias_remap_adapter`, `smn_inbound_remap_adapter`) simply use whatever value is currently stored.

### 7.6 Bugs Found and Fixed This Session

A compliance audit against `docs/new_rtls/sep_cpu_ctrl.rdl` (prompted by `sep_reference_counter_test.c` failing) found two independent defects:

1. **`REFERENCE_COUNTER` had no write support at all.** `handle_write_REFERENCE_COUNTER` didn't exist, and the register's own constructor passed `read_bit_mask=0, write_bit_mask=0` — even after adding the write callback, it would have ANDed every write with 0 had the masks not also been corrected to the full 64-bit field width. The RDL marks this field `sw=rw`/`wr_swacc=true` specifically because real hardware's free-running counter (`prim_refclk_count_w_cdc`) accepts a CDC-synced reload from software; `sep_reference_counter_test.c`'s step 2 ("write a distinctive value, confirm the counter reloaded to it") had nothing to reload against and timed out. Fixed by adding the write handler, correcting the masks, and — since the RDL also says the counter restarts at 0 "after deassert of cold reset" — zeroing `hwif_in.reference_counter_rc` in `reset_handler()` (it was previously untouched by `reset_all_registers()`, which only resets CSML-backed storage, not this struct field).
2. **A three-register, VP-wide address-map drift.** This model still carried `SPACC_CTRL` (0x30), `TIMEOUT_COUNT_TROOT` (0x40), and `TIMEOUT_COUNT_SPACC` (0x50) — all removed from real hardware in the RTL revision `docs/new_rtls` reflects — plus their matching bitfields (`spacc_cg_enable` in `CLOCK_GATE_CTRL`, `troot_timeout_int`/`spacc_timeout_int` in `TIMEOUT_INTERRUPT`/`TIMEOUT_ENABLE`/`TIMEOUT_CLEAR`/`TIMEOUT_MODE`, `fast_spacc_en` in `SEP_TEST_CTRL`). Confirmed against the live firmware header (`sw/tt-oca-hw-main/meta/registers/c/och_sep_top_reg.h`) that `SEP_CPU_CTRL_TIMEOUT_COUNT_DMA_REG_ADDR` is really `0x10A30028`, not this model's `0x10A30048` — meaning every `TIMEOUT_COUNT_*`/`TIMEOUT_ENABLE`/`TIMEOUT_CLEAR`/`TIMEOUT_MODE` register was listening 0x20–0x28 bytes off from where current firmware actually addresses it, and every bit past the removed fields in `CLOCK_GATE_CTRL`/`TIMEOUT_INTERRUPT`/`SEP_TEST_CTRL` was shifted by one. Fixed by removing the three phantom registers and fields, renumbering every remaining bit to match the RDL exactly, and adding `KM_WIPE_CTRL` (§2.1.9) — the one register genuinely new in this RTL revision that the old model was missing.

Both the live unit test (`test/sep_cpu_ctrl_test.cpp`) and the address/mask tables in the unused `test/*basetest*` scaffold were updated to match — the live test previously had an assertion that explicitly codified the `REFERENCE_COUNTER` write bug as expected behavior ("ignores SW writes"), plus several offset literals (`TIMEOUT_CLEAR` at `0x098`, `TIMEOUT_MODE` at `0x0A0`, a `TIMEOUT_COUNT` register at `0x040`) that pointed at now-unmapped or wrong-register addresses post-fix.

---

## 8. Conclusion

### 8.1 Summary

`sep_cpu_ctrl` is the SEP system CSR block that most other VP components depend on indirectly:

1. **Five live-read address-window registers** feed three separate downstream consumers (`local_alias_remap_adapter`, `smn_inbound_remap_adapter`, the SMU merge path) without any explicit change-notification — consumers simply re-read on every transaction.
2. **Two lock-gated fields** (`SEP_NMI_VEC`, `EXT_TRNG_SRC_SEL`) correctly implement one-way WOSET-style locking via private shadow state, since CSML's declarative register model has no native "conditionally reject a write" primitive.
3. **The `TIMEOUT_CLEAR`/`TIMEOUT_MODE` write-mask bug is fixed** — both registers are now genuinely writable; `TIMEOUT_CLEAR`'s singlepulse clear-and-self-reset logic is real and correct, even though nothing currently sets the flags it clears.
4. **Nine CCI-parameter hardware inputs** deliver strap/fuse-sense scenarios without recompilation, snapshotted once at `end_of_elaboration()`.
5. **`REFERENCE_COUNTER` is now genuinely writable and register-map-correct**, and the three-register/multi-field address-map drift from a stale RTL snapshot (`SPACC_CTRL`, `TIMEOUT_COUNT_TROOT`, `TIMEOUT_COUNT_SPACC`, and their bitfields) is gone — every register now sits at the address current firmware headers actually expect (§7.6).

### 8.2 Known Gaps

| Gap | Risk | Mitigation |
|-----|------|------------|
| `TIMEOUT_COUNT_*`/`TIMEOUT_ENABLE` have no counting/expiry logic | Low today (no test depends on it); would block any future timeout-behavior test | `hwif_in.*_timeout_int` backdoor exists for direct test-side poking; a real fix needs an `SC_METHOD`/`sc_spawn` counting mechanism |
| `KM_WIPE_CTRL` has no downstream Key Manager wipe consumer | Low today (no test depends on it); register-map-correct, just inert | Would need an actual Key Manager KPV-wipe model to react to a rising edge, which doesn't exist in this VP |
| `REFERENCE_COUNTER` increments on read, not on elapsed time (write/reload is correct as of this revision) | Low — matches all known current usage, including `sep_reference_counter_test.c`'s poll-until-advance pattern | Would need a clock-sensitive process if a test ever depends on real elapsed-time semantics |
| `smc_fuse_sense_done`/`sep_fuse_sense_done` hardcoded `true` in `och_sep_ss.hpp`, bypassing this peripheral's own correctly-built CCI-param mechanism | Low in a standalone SEP VP; blocks realistic SEP+SMC boot-sequence testing | Tracked in `Abstractions.md` §3.5; requires the top-level wiring to stop hardcoding these once a real signal source exists |
| No bounds checking on address-window registers | Low — firmware is trusted in this VP | N/A — would require explicit range validation if ever needed |
| `CLOCK_GATE_CTRL`/`PKA_CTRL`/`RAS_BANK_INFO`/`SEP_SW_DEBUG` are inert | None — informational-only registers in real hardware terms too, for this VP's purposes | N/A |

---

**End of High-Level Design Specification**
