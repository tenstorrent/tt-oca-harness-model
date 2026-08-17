# VeeR EL2 PIC SystemC TLM High-Level Design Specification

**Document Version:** 1.0
**Date:** 2026-07-27
**IP Module:** VeeR EL2 Programmable Interrupt Controller (`el2_pic`)
**Modeling Approach:** SystemC TLM2.0, behavioural gateway + arbitration model, internal to the CPU wrapper (not on the system bus)

---

## Executive Summary

This document specifies the SystemC Transaction-Level Model (TLM) for the VeeR EL2 core's embedded Programmable Interrupt Controller (`el2_pic`). Unlike every other peripheral in this VP, the PIC is **not a system-bus target** — it models a core-internal block that lives entirely inside the `VeeRISSTlm` CPU wrapper, mirroring how the real VeeR EL2 RTL instantiates its PIC as part of the core rather than as an external AXI/AXI-Lite peripheral.

### Key Architectural Characteristics

- **Split register model** — the real PIC's registers are split across two address spaces: RISC-V CSR space (`meivt`, `meipt`, `meihap`, `meicpct`, `meicidpl`, `meicurpl`) and a 32-bit memory-mapped MMIO space (`mpiccfg`, `meipl[S]`, `meip[X]`, `meie[S]`, `meigwctrl[S]`, `meigwclr[S]`). This model only implements the **memory-mapped half** as CSML registers; the CSR-space registers are modeled as plain state inside `VeeRISSTlm` itself and reached via `peek_csr()`/`poke_csr()`.
- **Per-source gateway processes** — one `sc_spawn`'d SystemC method per interrupt source (255 usable sources, ID 0 reserved), each sensitive to its own `irq_in[i]` signal, applying configurable polarity and level/edge semantics independently.
- **Direct hart hand-off, no bus transaction** — `el2_pic_model` holds a raw back-pointer (`hart_`) to `VeeRISSTlm` and calls its `trigger_external_interrupt()`/`clear_external_interrupt()`/`set_pic_claim_id()` methods directly on every arbitration change, rather than driving a TLM socket.
- **No priority stack** — nested-interrupt priority push/pop (save/restore `meicurpl` across ISR entry/exit) is firmware's responsibility per the real spec; the model does not implement any hardware priority stack, matching real hardware (this is correctly a firmware, not hardware, responsibility) but explicitly not re-verified for multi-level nesting scenarios beyond single-priority firmware.
- **255 usable interrupt sources** — this SEP instance is built with `RV_PIC_TOTAL_INT = 255` / `RV_PIC_TOTAL_INT_PLUS1 = 256` and `RV_PIC_INT_WORDS = 8` (`vendor/chipsalliance/Cores-VeeR-EL2/overlay/snapshots/sep/common_defines.vh:158,184,185`), so the model carries 256 slots — IDs 1–255 usable, ID 0 reserved per spec — and 8 pending words. Note the two sources that suggest otherwise and are *not* authoritative for this build: `meta/registers/rdl/el2_pic.rdl` carries a `PIC_TOTAL_INT = 32` parameter **default**, and the vendored `docs/01_el2_pic_Specification/pic.h` was generated with it (its highest name, `PIC_MEIPL_31` at offset `0x80`, is source **32**, since the RDL array starts at `@0x0004`). `el2_pic_ctrl.sv` sizes every array off `PIC_TOTAL_INT_PLUS1`, so the build parameter wins.

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Features](#2-features)
3. [Port Interfaces](#3-port-interfaces)
4. [Memory-Mapped Registers](#4-memory-mapped-registers)
5. [Behavioural Logic](#5-behavioural-logic)
6. [Internal Architecture](#6-internal-architecture)
7. [Modeling Assumptions](#7-modeling-assumptions)
8. [Conclusion](#8-conclusion)

---

## 1. Introduction

### 1.1 VeeR EL2 PIC Overview

The PIC (Programmable Interrupt Controller) is modeled closely after the RISC-V PLIC specification but, unlike a platform-level PLIC, it is instantiated **inside** the VeeR EL2 core and serves exactly **one interrupt target**: the hart's M-mode context. Its job:

1. **Gateway** — each of up to 255 external interrupt sources has a dedicated gateway that synchronizes the async request into the core clock domain and converts it into a level-triggered "pending" signal, applying configurable polarity and edge/level type.
2. **Arbitration** — the PIC core continuously evaluates all pending+enabled sources and picks the one with the highest priority (ties broken by lowest source ID), comparing it against the target's priority threshold(s).
3. **Hand-off** — if the winning source's priority exceeds threshold, the PIC asserts the hart's external interrupt line and exposes the winner's ID through `meihap` (via `meicpct`-triggered capture) for the trap handler to read.

This SEP integration builds the PIC with **255 sources**. SEP itself only drives the low end of that range — `sep_internal_interrupts[]` is the 38-entry array documented in `irq_map.h`, of which 31 are currently wired to `pic_inputs[]` — so the remaining slots exist and are addressable but sit tied low (§3.1.4).

### 1.2 Purpose of This Document

This high-level design specification defines the SystemC TLM implementation for virtual platform integration. It documents:

- Features included and excluded from the TLM model
- The unconventional port/socket topology (internal to `VeeRISSTlm`, not a bus target)
- Complete memory-mapped register specification
- Gateway and arbitration behavioural logic
- Modeling assumptions and abstractions, particularly around the CSR/MMIO register split

### 1.3 Modeling Goals

1. **Deliver correct external-interrupt semantics to firmware** — priority-based arbitration, gateway polarity/edge configuration, and claim-ID hand-off must match real hardware closely enough that unmodified SEP firmware ISR-dispatch code (`meicpct`/`meihap`-based vectoring) works unchanged.
2. **Immediate reactivity to configuration changes** — writes to `meipl`, `meie`, `meigwctrl`, and to the CSR-space `meipt`/`meicurpl` (via `notify_threshold_changed()`) all trigger re-arbitration synchronously, so firmware never needs to poll for a state that should already be reflected.
3. **Correct priority-order handling** — both standard (`priord=0`, 15=highest) and reverse (`priord=1`, 0=highest) priority order conventions are supported, matching the real RTL's bit-inversion behavior.
4. **No unnecessary bus-level modeling** — since the real PIC is core-internal, this model deliberately avoids exposing it on the SEP system bus; it is only reachable through `VeeRISSTlm`'s own dedicated internal socket.

### 1.4 Target Audience

- SystemC model developers maintaining or extending `el2_pic` or `VeeRISSTlm`
- VP architects reasoning about SEP's internal vs. external interrupt paths (see `irq_map.h`, `sep_internal_interrupts[]`)
- SEP firmware engineers writing interrupt handlers, vector tables, or threshold/priority configuration code
- Verification engineers creating or extending the test plan

---

## 2. Features

### 2.1 Is (MUST Model)

#### 2.1.1 Per-Source Gateway

Each source `1..255` has its own `sc_spawn`'d SystemC method (`gateway_changed`), sensitive to `irq_in[i].value_changed_event()`, deferred to `before_end_of_elaboration()` so port binding is guaranteed complete first. On every event:

- **Polarity** (`meigwctrl[S].polarity`): `effective = polarity ? !raw : raw`.
- **Level mode** (`irq_type=0`): `source_pending_[S]` tracks `effective` directly.
- **Edge mode** (`irq_type=1`): a latching gateway rather than a transition detector, matching `el2_pic_ctrl.sv:588-592`. `source_pending_[S]` is set on any evaluation where `effective` is asserted, so a narrow pulse survives until firmware sees it. Only a write to `meigwclr[S]` clears it, and only while `effective` is inactive — clearing a still-asserted source re-pends it immediately, since the RTL ORs the live level on top of the latch.
- `meip[0..7]` are not mirrored. Reads go through a callback that computes the bitmap from `source_pending_[]` on demand, matching the RTL where `intpend` is combinational off the gateways rather than a register.

#### 2.1.2 Priority Arbitration (`reevaluate_arbitration()`)

Re-run synchronously after any of: a gateway event, a `meipl`/`meie`/`meigwctrl` write, a `meigwclr` write, or an explicit `notify_threshold_changed()` call (used by `VeeRISSTlm` when firmware writes CSR `meipt`/`meicurpl`).

```
for s in 1..255:
    skip if !source_pending_[s]
    skip if MEIE[s].inten == 0
    eff_prio = priord ? (~raw_prio & 0xF) : raw_prio
    keep s if eff_prio > best_eff_prio       // strictly greater — ties keep the lower (earlier) ID
```

Note the absence of a raw `intpriority == 0` skip. "Priority 0 never interrupts" is not a special case in `el2_pic_ctrl.sv:329-331` — it falls out of the threshold compare below, because an effective priority of 0 cannot be strictly greater than a threshold of 0. Stating it as a rule on the *raw* value would be wrong under `priord=1`, where raw 0 inverts to 15 (the highest) and raw 15 becomes the value that can never win.

The winner is then compared against **both** threshold CSRs read live from the hart (`meipt` at 0xBC9, `meicurpl` at 0xBCC), each independently priority-order-adjusted:

```
blocked = (best_eff_prio <= meipt_eff) || (best_eff_prio <= meicurpl_eff)
```

If not blocked and the winner changed (or the IRQ line wasn't already asserted), the model:
1. Calls `hart_->set_pic_claim_id(best_id)`.
2. Pokes `meicidpl` (CSR 0xBCB) with the winning effective priority — mirroring real RTL's `dec_tlu_ctl.sv` behavior of capturing `meicidpl ← pic_pl` on `take_ext_int_start` (note: `meicurpl` is never auto-updated by hardware in the real design either — it is firmware-only, set explicitly via `csrw`).
3. Calls `hart_->trigger_external_interrupt(MachineMode)`.

If no source wins (or the winner drops below threshold), and the IRQ line is currently asserted, it calls `hart_->clear_external_interrupt(MachineMode)`.

#### 2.1.3 Priority-Order Convention (`mpiccfg.priord`)

Both standard (`priord=0`: 0=never-interrupt, 1..15 increasing priority) and reverse (`priord=1`: 15=never-interrupt, 14..0 increasing priority) conventions are supported by bitwise-inverting the raw 4-bit priority field before comparison — matching the real RTL's documented approach of always evaluating in one internal order and inverting register read/write paths for firmware-visible reverse-order semantics.

#### 2.1.4 `meigwclr[S]` — Write-Only Gateway Clear

Overrides the default CSML write callback entirely (registers a custom write callback rather than relying on `handle_write`, since this register has no underlying storage semantics): any write clears `source_pending_[S]`, then re-evaluates that source (level-mode sources may immediately re-latch if the input is still asserted) and re-runs arbitration. Reads always return 0.

#### 2.1.5 `meip[0..7]` — Live-Computed, Not Stored

Overrides both read and write callbacks: writes are silently dropped (RO to software), and reads recompute the pending bitmap live from `source_pending_[]` via `pending_word(w)` rather than trusting the backing `csml_memory` word. Bit Y of word X is source X*32+Y, matching `el2_pic_ctrl.sv:488-491`, where the word index is decoded from address bits `[5:2]`.

Nothing is mirrored back into `MEIP` storage. `csml`'s `read_registers()` consults the read callback on the debug path as well as the functional one (`csml_register.h:359`, reached from `transport_dbg`), so a stored copy would never be observable — it would be state that can silently drift with no test able to catch it. This also matches the RTL, where `intpend` is combinational off the gateways rather than a register.

#### 2.1.6 Reset Behavior

`SC_METHOD` sensitive to `rst_ni.neg()`: resets all CSML registers to power-on defaults, clears `source_pending_[]`, and — if an external interrupt was asserted — calls `hart_->clear_external_interrupt()` before dropping `eip_asserted_`.

#### 2.1.7 Internal (Non-Bus) Socket Topology

`el2_pic_base::target_socket` is bound directly to `VeeRISSTlm`'s own dedicated `pic_isock_` initiator socket at construction time — the PIC is a **member object of `VeeRISSTlm`**, not a peer module on the SEP `SimpleBus`. `och_sep_ss.hpp` never references `el2_pic` directly for exactly this reason (see also: `el2_pic_model` is only reachable transitively through `veeriss_model`'s `PUBLIC` CMake link).

### 2.2 Is Not (EXCLUDE)

#### 2.2.1 CSR-Space PIC Registers

`meivt` (0xBC8), `meipt` (0xBC9), `meicpct` (0xBCA), `meicidpl` (0xBCB), `meicurpl` (0xBCC), and `meihap` (0xFC8) are **not CSML registers owned by `el2_pic`** — they are plain state inside `VeeRISSTlm`, reached from `el2_pic` only via `peek_csr()`/`poke_csr()`/`set_pic_claim_id()`. This split matches the real hardware's own CSR-vs-MMIO register-space division, but means this document's "Memory-Mapped Registers" section (§4) is deliberately incomplete relative to the full PIC register set described in the real specification — the CSR half is out of scope for `el2_pic` and belongs to `VeeRISSTlm`'s own design documentation.

#### 2.2.2 Priority Stack / Nested-Interrupt Save-Restore

The real spec's nested-interrupt flow (firmware pushes `meicurpl`, sets it to the new handler's priority, re-enables interrupts, then pops on exit) is entirely firmware-driven — there is no hardware stack to model. The VP model does not add any hardware-side nesting support beyond correctly re-arbitrating whenever `meicurpl` changes (via `notify_threshold_changed()`); it has not been separately verified against a firmware test that exercises multiple simultaneous nesting levels.

#### 2.2.3 Fast Interrupt Redirect

The real VeeR EL2 build-time option that lets hardware auto-capture claim ID/priority and redirect execution directly to the ISR (bypassing firmware's `meicpct`/`meihap` read sequence) via a DCCM vector-table walk is not modeled as a distinct hardware fast-path. `VeeRISSTlm` still requires firmware (or its own trap-handling code) to read `meihap` to obtain the vector.

#### 2.2.4 Interrupt Chaining

Not separately implemented — chaining is a firmware technique (repeatedly capture-and-check `meihap` at the end of an ISR before restoring state) that works "for free" against this model's continuous arbitration, with no dedicated hardware support required or added.

#### 2.2.5 Power Reduction (Gateway Clock Gating)

The real RTL's `picio` bit (in `mcgc`) that clock-gates groups of 4 gateways together when all four sources are disabled is a pure power-domain feature — not modeled at all; irrelevant at this TLM abstraction level (no power/clock-gating simulation).

#### 2.2.6 Wake-Up Notification (WUN / `mhwakeup`)

The wake-from-Sleep signal driven when the winning priority reaches the hardwired maximum threshold is not modeled — this VP does not model core power/sleep states.

#### 2.2.7 Timing

- Gateway synchronizer latency (2-cycle minimum pulse width for async sources) — not modeled; `sc_spawn` reacts to any signal edge immediately.
- The documented 4-cycle PIC target latency — not modeled; arbitration and hand-off are logically instantaneous (`SC_ZERO_TIME`-equivalent, driven by direct C++ calls rather than a timed TLM transaction).

---

## 3. Port Interfaces

| Category | Port/Socket Name | Type | Direction | Description |
|----------|-------------------|------|-----------|--------------|
| **Register Bus** | `target_socket` (via `el2_pic_base`) | `tlm_utils::simple_target_socket<csml_memory<32>, 32>` | Target | Bound directly to `VeeRISSTlm`'s internal `pic_isock_` — **not** a SEP `SimpleBus` target. Backing memory window: `0x8000` bytes. |
| **Clock/Reset** | `clk_i`, `rst_ni` | `sc_in<bool>` | Input | `rst_ni` drives `reset_process()` on the negative edge; `clk_i` is present but not used for any clocked logic in this LT model (all logic is event/callback-driven). |
| **Interrupt Sources** | `irq_in` | `sc_vector<sc_in<bool>>`, size `NUM_INTERRUPTS=256` | Input | Index 0 unused (reserved per spec); indices 1–255 are the interrupt sources. A parent binds only the sources it drives — `before_end_of_elaboration()` ties the rest low (see §3.1.4). Today `och_sep_ss` binds indices 0–31 per `irq_map.h`. |
| **Hart back-reference** | `bind_hart(VeeRISSTlm*)` | C++ method call | — | Not a TLM/SystemC port — a raw pointer stored in `hart_`, used to call `trigger_external_interrupt()`/`clear_external_interrupt()`/`set_pic_claim_id()`/`peek_csr()`/`poke_csr()` directly. |
| **Threshold change notification** | `notify_threshold_changed()` | C++ method call | — | Called by `VeeRISSTlm`'s CSR-write instrumentation when firmware writes `meipt` (0xBC9) or `meicurpl` (0xBCC), so a threshold change takes effect on the very next arbitration rather than waiting for the next gateway event. |

### 3.1 Interface Notes

1. **No bus address** — `el2_pic` has no notion of a system-bus base address at all (unlike every other SEP peripheral); its `0xC008_0000` base (per `pic.h`) is purely a VeeR-EL2-internal MMIO convention used by firmware's own load/store instructions, resolved entirely within `VeeRISSTlm`'s internal socket, never touching `och_sep_ss`'s `SimpleBus`.
2. **`bind_hart()` must be called during elaboration** before any interrupt activity — the model asserts nothing if `hart_ == nullptr` (arbitration silently returns without driving anything).
3. **CMake visibility** — `el2_pic_model` requires no entry in `vp/platform/sep/CMakeLists.txt`; it is pulled in transitively through `veeriss_model`'s `PUBLIC` link dependency (see `sep/cpu/CMakeLists.txt`).
4. **Unbound sources are tied low** — `before_end_of_elaboration()` binds any `irq_in[i]` the parent left open to an internal always-low signal, before spawning the gateways. This is what decouples the PIC's width from its integrations: SystemC requires every `sc_in` to be bound, so without it the gateway spawn would fail with E112 on the first unbound port and `sc_start()` would abort with E109. A parent's own binding always takes precedence; the tie-off only fills gaps. A tied-low source is never pending under the reset gateway config (level mode, active-high).

---

## 4. Memory-Mapped Registers

`el2_pic` exposes one `0x8000`-byte memory window (internal to `VeeRISSTlm`, base address per `pic.h`: `0xC008_0000`, real MMIO surface ending at source 255's `meigwclr` = offset `0x53FC`–`0x53FF`). The window spans the core's full `pic_size = 32` KiB aperture rather than stopping at the last implemented register, so offsets above `meigwclr` read back as zero instead of erroring.

### 4.1 PIC Configuration Register

| Register | Offset | Reset | Access | Description |
|----------|--------|-------|--------|-------------|
| **MPICCFG** | 0x3000 | 0x00000000 | RW | Bit[0] `priord`: 0=standard priority order (15=highest), 1=reverse (0=highest). Bits[31:1] reserved. Plain CSML storage — no callback needed; arbitration reads it live on every `reevaluate_arbitration()` call. |

### 4.2 Per-Source Priority Level Registers (MEIPL)

| Register | Offset | Reset | Access | Description |
|----------|--------|-------|--------|-------------|
| **MEIPL[0]** | 0x0000 | 0x0 | — | Reserved (source ID 0 invalid). |
| **MEIPL[1..255]** | 0x0004–0x03FC | 0x0 each | RW | Bits[3:0] `intpriority`: priority, with both the ordering and which value means "never interrupt" depending on `MPICCFG.priord` — raw 0 under `priord=0`, raw 15 under `priord=1`. Bits[31:4] reserved. Every write triggers `post_write_MEIPL()` → `reevaluate_arbitration()`. |

### 4.3 Pending Registers (MEIP)

| Register | Offset | Reset | Access | Description |
|----------|--------|-------|--------|-------------|
| **MEIP[0..7]** | 0x1000–0x101C | 0x0 each | RO (live-computed) | Bit `Y` of word `X` = pending state of source `X*32+Y`, covering sources 0–255. Overridden read callback recomputes from `source_pending_[]` on every access; writes silently dropped. Eight words per `RV_PIC_INT_WORDS = 8`. |

### 4.4 Per-Source Enable Registers (MEIE)

| Register | Offset | Reset | Access | Description |
|----------|--------|-------|--------|-------------|
| **MEIE[0]** | 0x2000 | 0x0 | — | Reserved. |
| **MEIE[1..255]** | 0x2004–0x23FC | 0x0 each | RW | Bit[0] `inten`: 1=source enabled. Bits[31:1] reserved. Every write triggers `post_write_MEIE()` → `reevaluate_arbitration()`. |

### 4.5 Per-Source Gateway Configuration Registers (MEIGWCTRL)

| Register | Offset | Reset | Access | Description |
|----------|--------|-------|--------|-------------|
| **MEIGWCTRL[0]** | 0x4000 | 0x0 | — | Reserved. |
| **MEIGWCTRL[1..255]** | 0x4004–0x43FC | 0x0 each | RW | Bit[0] `polarity` (0=active-high, 1=active-low), Bit[1] `irq_type` (0=level, 1=edge). Bits[31:2] reserved. Every write triggers `post_write_MEIGWCTRL()` → recompute this source's pending state under the new rules → `reevaluate_arbitration()`. |

### 4.6 Per-Source Gateway Clear Registers (MEIGWCLR)

| Register | Offset | Reset | Access | Description |
|----------|--------|-------|--------|-------------|
| **MEIGWCLR[0]** | 0x5000 | — | — | Reserved. |
| **MEIGWCLR[1..255]** | 0x5004–0x53FC | — | WO (custom callback, no storage) | Any write clears `source_pending_[S]`; reads always return 0. |

---

## 5. Behavioural Logic

### 5.1 Event-Driven Recompute Chain

Every stateful change funnels into `reevaluate_arbitration()`:

```
irq_in[i] changes      → gateway_changed(i) → recompute_pending_for_source(i) → reevaluate_arbitration()
MEIPL[S] write          → post_write_MEIPL(S)                                  → reevaluate_arbitration()
MEIE[S] write           → post_write_MEIE(S)                                   → reevaluate_arbitration()
MEIGWCTRL[S] write      → post_write_MEIGWCTRL(S) → recompute_pending_for_source(S) → reevaluate_arbitration()
MEIGWCLR[S] write       → write_MEIGWCLR(S)        → (recompute if level mode)      → reevaluate_arbitration()
CSR meipt/meicurpl write (in VeeRISSTlm) → notify_threshold_changed()          → reevaluate_arbitration()
```

There is no polling anywhere in this chain — every firmware-visible state change is reflected synchronously.

### 5.2 Duplicate-Suppression on Hand-Off

`eip_asserted_` and `current_claim_id_` track the last state driven to the hart, so `reevaluate_arbitration()` only calls `trigger_external_interrupt()`/`set_pic_claim_id()`/`poke_csr()` again when the winning ID actually changes (or the line wasn't previously asserted) — repeated arbitration passes that produce the same winner are no-ops toward the hart.

---

## 6. Internal Architecture

### 6.1 Class Hierarchy

```
el2_pic::el2_pic_model (sc_module, extends el2_pic_base)
├── el2_pic_base                    (CSML register instances + target_socket)
│   ├── MPICCFG                     @ 0x3000
│   ├── MEIPL[0..255]               @ 0x0000 (idx0 reserved)
│   ├── MEIP[0..7]                  @ 0x1000
│   ├── MEIE[0..255]                @ 0x2000 (idx0 reserved)
│   ├── MEIGWCTRL[0..255]           @ 0x4000 (idx0 reserved)
│   └── MEIGWCLR[0..255]            @ 0x5000 (idx0 reserved)
├── csml_param<int>                 verbosity
├── sc_in<bool>                     clk_i, rst_ni
├── sc_vector<sc_in<bool>>          irq_in  [256]
├── CsmlLogger                      logger
└── (private)
    ├── VeeRISSTlm*                 hart_ = nullptr   — set via bind_hart()
    ├── sc_vector<sc_signal<bool>>  irq_tie_low_      — sized in before_end_of_elaboration
    ├── std::array<bool,256>        source_pending_
    ├── bool                        eip_asserted_ = false
    └── unsigned                    current_claim_id_ = 0
```

Owning relationship (this is the unusual part relative to every other SEP peripheral):

```
VeeRISSTlm (sc_module)
├── el2_pic::el2_pic_model  pic_          — owned by value, not new'd/bound externally
├── tlm_utils::simple_initiator_socket<VeeRISSTlm, 32>  pic_isock_   — bound to pic_.target_socket internally
└── pic_.bind_hart(this)                  — called during VeeRISSTlm's own construction
```

### 6.2 Initialization Flow

```
el2_pic_model constructor:
  → el2_pic_base constructor: allocate csml_memory<32>, construct all register objects, bind target_socket
  → source_pending_.fill(false)
  → SC_METHOD(reset_process), sensitive to rst_ni.neg()
  → register_all_callbacks()   — install post-write / custom read/write callbacks

before_end_of_elaboration():
  → irq_tie_low_.init(NUM_INTERRUPTS)
  → for each source 0..255:
      if irq_in[i] was left unbound by the parent: bind it to irq_tie_low_[i] (driven false)
      sc_spawn a method sensitive to irq_in[i].value_changed_event()
    (deferred here specifically because port binding must already be complete —
     calling value_changed_event() on an unbound sc_in raises a SystemC error,
     which is also why the tie-off has to happen in the same pass, before the spawn)

VeeRISSTlm construction (external to el2_pic):
  → pic_isock_.bind(pic_.target_socket)
  → pic_.bind_hart(this)
```

### 6.3 Arbitration Data Flow (per event)

```
gateway/register event
  → recompute_pending_for_source(s)  [if a gateway-relevant change]
       raw = irq_in[s].read()
       effective = polarity ? !raw : raw
       edge mode:  latch true whenever effective is asserted
       level mode: source_pending_[s] = effective
  → reevaluate_arbitration()
       scan s=1..255, skip !pending / !enabled
       eff_prio = priord ? ~raw_prio : raw_prio   (masked to 4 bits)
       keep strictly-greater eff_prio (ties keep lower/earlier ID)
       if winner found:
           read meipt, meicurpl live from hart_ (peek_csr)
           if winner's eff_prio > both thresholds:
               hart_->set_pic_claim_id(winner)
               hart_->poke_csr(meicidpl, winner_eff_prio)
               hart_->trigger_external_interrupt(MachineMode)
           else:
               hart_->clear_external_interrupt(MachineMode)  [if previously asserted]
       else:
           hart_->clear_external_interrupt(MachineMode)  [if previously asserted]
```

---

## 7. Modeling Assumptions

### 7.1 Register-Space Split

1. **CSR-space registers live in `VeeRISSTlm`, not here** — `el2_pic` treats `meivt`/`meipt`/`meihap`/`meicpct`/`meicidpl`/`meicurpl` as external state, reached only via `peek_csr()`/`poke_csr()`. Anyone auditing "the PIC's registers" against the real specification must look at both `el2_pic_register.h` (MMIO half) and `VeeRISSTlm`'s own CSR handling (CSR half) to get the complete picture — this document only covers the former.

2. **`meicurpl` is never auto-updated by hardware** — matches real RTL exactly (confirmed via `dec_tlu_ctl.sv` comment in `el2_pic.cpp`): only `meicidpl` is hardware-captured on `take_ext_int_start`; `meicurpl` is purely firmware-managed via explicit `csrw`.

### 7.2 Arbitration Semantics

3. **Tie-breaking by lowest ID falls out of loop order, not explicit logic** — the `>` (strictly greater) comparison in `reevaluate_arbitration()`'s ascending loop naturally keeps the first (lowest-ID) source when multiple sources share the same effective priority, matching the spec's "lowest interrupt source ID" tie-break rule without needing a separate explicit check.

4. **Threshold reads are always live** — `meipt`/`meicurpl` are read via `peek_csr()` fresh on every arbitration pass rather than cached, so there is no possibility of a stale-threshold bug, at the cost of a "poke into the hart" dependency that makes `el2_pic` unusable without a bound `hart_`.

### 7.3 Source Count and IDs

5. **256 total slots, 255 usable (IDs 1–255)** — `NUM_INTERRUPTS=256` is a fixed compile-time constant matching this SEP integration's `RV_PIC_TOTAL_INT_PLUS1` build parameter (`common_defines.vh:185`), with `NUM_PEND_WORDS=8` matching `RV_PIC_INT_WORDS`. ID 0 is permanently reserved and skipped everywhere (arbitration loop, callback registration loop).

   The model previously used 32, taken from the `PIC_TOTAL_INT` **default** in `el2_pic.rdl` and from the vendored `pic.h` generated with it. That was doubly wrong: it dropped sources 33–255 entirely, and because the RDL's arrays start at `@0x0004` while this model indexes by source ID from `0x0000`, a 32-entry array stopped one register short of even `pic.h`'s own last name (`PIC_MEIPL_31` at offset `0x80`, which is source 32). When changing this constant, note it is the IP half of a pair: `PIC_NUM_INTERRUPTS` in `vp/platform/sep/inc/irq_map.h` sizes the platform's `pic_inputs` vector and bounds its binding loop, so sources above that bound cannot be *routed* until it moves too. The tie-off in §3.1.4 is what keeps the two independent enough to change in separate PRs.

6. **All sources assume a configurable gateway is present** — the real spec notes `meigwctrlS`/`meigwclrS` "are only present if a configurable gateway is instantiated" per source; this model always instantiates all 255, matching `el2_pic_ctrl.sv`, which sizes `gw_config_reg` off `PIC_TOTAL_INT_PLUS1` with no per-source opt-out.

### 7.4 Timing

7. **All logic is effectively zero-delay** — gateway recompute and arbitration are plain C++ function calls triggered by SystemC event notifications, with no explicit `sc_time` delay anywhere in the chain. The documented 4-cycle real-hardware PIC latency (including gateway CDC) is not reproduced.

---

## 8. Conclusion

### 8.1 Summary

The `el2_pic` SystemC TLM model provides a firmware-correct, event-driven implementation of VeeR EL2's embedded interrupt controller's memory-mapped register half, tightly coupled to `VeeRISSTlm` rather than exposed on the SEP system bus. Key aspects:

1. **Core-internal topology** — bound directly to `VeeRISSTlm`'s own socket; invisible to `och_sep_ss`'s `SimpleBus` and to `vp/platform/sep/CMakeLists.txt` (pulled in transitively via `veeriss_model`'s public CMake link).
2. **Per-source gateway processes** — 255 independently-configurable sources with polarity + level/edge semantics, `sc_spawn`'d after port binding completes.
3. **Synchronous, poll-free arbitration** — every relevant register write or gateway event triggers immediate re-arbitration; live threshold reads from the hart's CSR state avoid stale-threshold bugs.
4. **Correct priority-order and tie-break semantics** — standard/reverse order via bit-inversion; lowest-ID tie-break falls naturally out of loop structure.
5. **Deliberately partial register scope** — only the MMIO half of the real PIC register set; the CSR half is `VeeRISSTlm`'s responsibility.

### 8.2 Known Gaps

| Gap | Risk | Mitigation |
|-----|------|------------|
| No hardware priority stack for nested interrupts | Low for single-priority firmware (current SEP tests); unverified for deep nesting | Firmware-managed nesting works via `notify_threshold_changed()` re-arbitration; no dedicated multi-level nesting test exists yet |
| Fast interrupt redirect not modeled as a distinct hardware path | Low — VP firmware already uses `meicpct`/`meihap` read sequence | None needed unless a test specifically targets the fast-redirect NMI failure modes |
| Gateway CDC/synchronizer timing not modeled | Low — functional VP, not cycle-accurate | N/A |
| Power reduction (gateway clock gating) not modeled | None — no power modeling in this VP | N/A |
| Wake-up notification (WUN/`mhwakeup`) not modeled | Low — no Sleep/pmu state modeling in this VP | N/A |
| CSR-space registers documented separately (in `VeeRISSTlm`, not here) | Low — by design | Cross-reference `VeeRISSTlm`'s own documentation for `meivt`/`meipt`/`meihap`/`meicpct`/`meicidpl`/`meicurpl` |

---

**End of High-Level Design Specification**
