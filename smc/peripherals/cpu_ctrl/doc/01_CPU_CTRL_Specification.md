# SMC CPU Control — Functional Specification

**Document**: `01_CPU_CTRL_Specification.md`  
**Module**: `smc::cpu_ctrl` (SystemC/TLM-2.0 Loosely-Timed model, CCI-parameterised)  
**Spec base**: `tt-oca-hw/hw/smc/smc_misc/data/registers/rdl/cpu_ctrl.rdl`  
**Handoff contract**: `tt-oca-hw/fw/smc/prod_rom/specification/smc_rom.adoc` § Scratch Registers  
**Status**: Frozen for SMC/SEP bring-up simulation  

---

## 1. Purpose & scope

The **CPU Control** block is the SMC fabric-control register file at
`BASE + 0x001_0000` (8 KiB).  Firmware uses it for cluster reset control,
address-space configuration, debug visibility, and — critically — the
**SCRATCH[16]** mailbox that coordinates **SMC ROM ↔ SEP ROM** boot handoff.

This specification defines the **externally observable behaviour** of the
SystemC LT model: register map, access widths, reset values, handoff indices,
and TLM bus contract.

---

## 2. Memory map summary

| Offset   | Register(s)              | SW access | Reset highlight |
|----------|--------------------------|-----------|-----------------|
| `0x000`  | `RESET_VECTOR[4]`        | RW        | `0xC004_0000` per core |
| `0x020`  | `RESET_CTRL`             | RW        | Cores/uncore out of reset |
| `0x028`  | `CORE_RESET_PULSE_COUNT` | RW        | Pulse counts + `core_resets_done=0xF` (RO) |
| `0x030`  | `CLOCK_GATE_CONTROL`     | RW        | `cg_hysteresis=0x1F` |
| `0x040`  | `GLOBAL_BASE`            | RW        | `0x4000_0000` |
| `0x048`  | `LOCAL_BASE`             | RO        | `0xC000_0000` |
| `0x050`  | `WDT_TIMEOUT`            | RW        | `0x4000` (RDL-aligned) |
| `0x058`  | `WDT_TIMEOUT_RESET`      | RW pulse  | Self-clearing |
| `0x060`  | `REFERENCE_COUNTER`      | RW        | `0` |
| `0x068`  | `REGION_SIZE`            | RW        | `0x0100_0000` (16 MiB; legacy) |
| `0x100`  | **`SCRATCH[16]`**        | RW        | `0` — **inter-stage handoff** |
| `0x200`  | `TEST_CTRL`              | RO        | HW backdoor |
| `0x208`  | `DEBUG_CTRL`             | RW        | `0` |
| `0x210`  | `DEBUG_BUS_MUX`          | RW        | `0` |
| `0x300`  | `WB_PC_COREn[8]`         | RO        | HW backdoor per core |
| `0x1000` | `SMC_ATTRIBUTES`         | RO        | HW backdoor (strap image) |
| `0x1040` | `MUTEX[4]`               | RW        | `1` (available) |
| `0x1060` | `SEMA[4]`                | RW        | Signed inc/dec on write |
| `0x1180` | `DUMMY_ROM_*`            | RW        | WFI loop patch words |

Holes inside the 8 KiB window are **RAZ/WI**.  Addresses `≥ 0x2000` return
`TLM_ADDRESS_ERROR_RESPONSE`.

When accessed through **`smc_fabric`**, offsets `0x40` / `0x48` / `0x50`
(`GLOBAL_BASE`, `LOCAL_BASE`, `REGION_SIZE`) may be intercepted by the fabric
instead of reaching this model.  Standalone benches and `to_cpu_ctrl` binding
use this block directly.

---

## 3. Inter-stage handoff (SCRATCH)

Base: **`0xC001_0100`** (`SMC_SCRATCH_BASE_ADDR` with `LOCAL_BASE = 0xC000_0000`).

Each scratch entry is a **64-bit register**; firmware uses **`data[31:0]`**.

| Index | Address      | Firmware symbol | Meaning |
|-------|--------------|-----------------|---------|
| 8     | `0xC001_0140` | `SMC_SCRATCH_MANIFEST_ADDR` | Manifest SRAM **offset** |
| 9     | `0xC001_0148` | `SMC_SCRATCH_SMC_STATUS_TO_SEP` | Status bits to SEP |
| 11    | `0xC001_0158` | `SMC_SCRATCH_STATUS_BUFFER_ADDR` | Status ring-buffer **offset** |
| 13    | `0xC001_0168` | `SMC_SCRATCH_SEP_SAFE_SRAM_START` | SEP safe region start **offset** |
| 14    | `0xC001_0170` | `SMC_SCRATCH_SEP_SAFE_SRAM_SIZE` | SEP safe region **size** |
| 15    | `0xC001_0178` | `SMC_SCRATCH_MEM_REPAIR_STATUS` | Memory-repair magic status |

**Status register 9 — bit definitions**

| Bit | Name | Meaning when set |
|-----|------|------------------|
| 0 | `SMC_SEP_STATUS_SRAM_INIT` | SMC SRAM initialized |
| 1 | `SMC_SEP_STATUS_MANIFEST_READY` | Register 8 valid |
| 2 | `SMC_SEP_STATUS_BUFFER_READY` | Register 11 valid |
| 3 | `SMC_SEP_STATUS_SRAM_PROTECTED` | SRAM protected |

**Register 15 — memory repair magic values**

| Value | Meaning |
|-------|---------|
| `0x600DCAFE` | Passed / not required |
| `0xBADC0FFE` | Failed |
| `0x12340001` | Bypassed (debug strap) |

---

## 4. Bus interface

- **Standard**: TLM-2.0 blocking transport (`b_transport`), Loosely-Timed.
- **Widths**: Naturally aligned `{1, 2, 4, 8}`-byte accesses.
- **Delay**: `access_delay_ns` CCI param (default **2 ns**) added to annotated delay.
- **DMI**: Never granted.
- **Addressing**: Accepts **IP-relative offsets** or **absolute** addresses in
  `[base_addr, base_addr + 0x2000)` (`base_addr` default `0xC001_0000`).

---

## 5. Special register behaviour

| Register | Model behaviour |
|----------|-----------------|
| `LOCAL_BASE`, `TEST_CTRL`, `SMC_ATTRIBUTES`, `WB_PC_*` | SW read-only; HW updates via backdoor API |
| `MUTEX[n]` | Read attempts acquire (returns 1 if free, 0 if held); any write releases |
| `SEMA[n]` | Write adds signed 16-bit delta to counter |
| `WDT_TIMEOUT_RESET`, `RESET_CTRL` pulse bits | Single-pulse: written bits self-clear |
| `CORE_RESET_PULSE_COUNT[35:32]` | Always reads `0xF` |

Functional reset sequencing (core reset pulses, clock gating side effects) is
**not** modeled — storage and documented pulse self-clear only.

---

## 6. CCI parameters

| Name | Type | Default | Purpose |
|------|------|---------|---------|
| `base_addr` | `uint64_t` | `0xC0010000` | Absolute CPU-control base |
| `access_delay_ns` | `double` | `2.0` | LT annotated access latency |

---

## 7. Revision history

| Date | Change |
|------|--------|
| 2025-06 | Initial LT model + handoff scratch coverage |
