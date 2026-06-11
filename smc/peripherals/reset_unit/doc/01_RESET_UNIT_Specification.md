# SMC Reset Unit — Specification (External Contract)

**Status:** Modelled · **Style:** SystemC / TLM-2.0 Loosely-Timed (LT) ·
**CCI:** 1.0

This document is the **external, RTL-traceable contract** of the SMC Reset
Unit SystemC model.  It describes *what* the model does (register map,
observable behaviour, error responses) without prescribing *how* — see
`02_RESET_UNIT_LowLevel_Design.md` for the internal design.

---

## 1. Role

The Reset Unit is the central reset controller of the System Management
Controller (SMC).  It:

1. Derives the chip-level **cold**, **primary**, **core**, and **WDT** resets
   from the pad/power inputs, watchdog timers, fuse-sensing status, and the
   incoming cool reset.
2. Distributes each reset, synchronised, to the **reference**, **SMC**, and
   **peripheral** clock domains.
3. Drives a **per-subsystem reset-control bundle** (`reset_ctrl_t`) for each
   of the 32 SMC subsystems, with software programming and JTAG override.
4. Implements the PCIe **Function-Level-Reset (FLR) "cool reset"** flow:
   isolate-request generation plus a timed `rst_cool_no` pulse to downstream
   chiplets.
5. Exposes a **32-bit AXI-Lite register block** to firmware.

RTL references: `smc_reset_unit.sv`, `smc_reset_ctrl.sv`,
`smc_subsystem_resets.sv`, `smc_cool_reset_wrap.sv`, `smc_reset_sync.sv`,
`reset_unit.rdl`, `smc_pkg.sv`.

---

## 2. Address window

| Property            | Value                                              |
|---------------------|----------------------------------------------------|
| Access size         | 32-bit only, naturally aligned                     |
| Window size         | `0x100` (8-bit decoded address; `RESET_UNIT_REG_SIZE = 0xCC`) |
| SMC base address    | `0xC000_2000` (`smc_top.rdl`; offsets are window-relative) |
| Endianness          | Little-endian                                       |

---

## 3. Register map (from `reset_unit.rdl`)

| Offset | Name                                  | SW  | Reset       | Description |
|--------|---------------------------------------|-----|-------------|-------------|
| 0x20   | `SS_CONFIG`                           | rw  | 0x00000000  | Per-SS configuration; locked bits (per `SS_CONFIG_LOCK`) are write-protected. Drives `ss_config_o`. |
| 0x24   | `SS_CONFIG_LOCK`                      | rw  | 0x00000000  | **woset**: write-1-to-set sticky lock for `SS_CONFIG`. |
| 0x40   | `SS_COLD_RESET_N`                     | rw  | 0x00000000  | Per-SS active-low cold reset; locked bits (per `SS_COLD_RESET_LOCK`) write-protected. |
| 0x44   | `SS_WARM_RESET_N`                     | rw  | 0xFFFFFFFF  | Per-SS active-low warm reset. |
| 0x48   | `SS_CONFIG_HOLD`                      | rw  | 0x00000000  | Preserve boot config across warm reset. |
| 0x4C   | `SS_SRAM_HOLD`                        | rw  | 0x00000000  | Preserve SRAM/MBIST state across warm reset. |
| 0x50   | `SS_CRITICAL_HOLD`                    | rw  | 0x00000000  | Preserve security/RAS/debug across warm reset. |
| 0x54   | `SS_DEBUG_HOLD`                       | rw  | 0x00000000  | Preserve debug registers across warm reset. |
| 0x60   | `SS_RESET_COMPLETE`                   | r   | 0x00000000  | Per-SS reset-complete status (driven by `ss_reset_complete_i`). |
| 0x70   | `SS_COLD_RESET_LOCK`                  | rw  | 0x00000000  | **woset**: write-1-to-set sticky lock for `SS_COLD_RESET_N`. |
| 0x80   | `SS_FORCE_TO_REF_CLK`                 | rw  | 0x00000000  | Force subsystem clock to ref clk during reset. |
| 0x90   | `STRAPS_LO`                           | r   | (HW)        | Captured straps `[31:0]` (`captured_straps_i[31:0]`). |
| 0x94   | `STRAPS_HI`                           | r   | (HW)        | Captured straps `[63:32]` (`captured_straps_i[63:32]`). |
| 0xA8   | `SYNC_REG`                            | rw  | 0x00000000  | bit0: global sync IRQ → `sync_irq_o`. |
| 0xB0   | `ISOLATE_REQ_REG`                     | rw  | 0x00000000  | SW per-SS isolate request. |
| 0xB4   | `ISOLATE_REQ_PINEN_REG`               | rw  | 0x00000000  | Enable external pin → isolate. |
| 0xB8   | `ISOLATE_REQ_SMC_REG`                 | rw  | 0x00000000  | bit0: **HW-set** on FLR rising edge; any SW write clears it. |
| 0xBC   | `ISOLATE_REQ_SMCEN_REG`               | rw  | 0x00000000  | Enable FLR latch → isolate. |
| 0xC0   | `ISOLATE_REQ_VIS`                     | r   | (HW)        | bit0 = isolate pin, bit4 = `rst_cool_ni`, bit8 = `rst_cool_no`. |
| 0xC4   | `ISOLATE_REQ_FLR_COUNTER_VALUE`       | rw  | 0x00000000  | Cycles after FLR before `rst_cool_no` asserts. |
| 0xC8   | `ISOLATE_REQ_FLR_RESET_COUNTER_VALUE` | rw  | 0x00000000  | `rst_cool_no` assertion duration (cycles). **0 ⇒ FLR pulse suppressed.** |

Holes inside the window are **RAZ/WI**.

---

## 4. Reset-derivation contract

All resets are active-low.  With JTAG overrides cleared:

```
powergood_stable   = powergood_i
fuse_reset_n       = fuse_reset_ni
cold_reset_n       = rst_cold_ni
stable_cold_rst_n  = powergood_stable & cold_reset_n
stable_cool_rst_n  = rst_cool_ni
wdt_reset_n        = rst_ext_wdt_ni & ~smc_wdt_second_timeout_i
rst_primary_n      = stable_cold_rst_n & stable_cool_rst_n & cool_from_flr_n
rst_core_n         = wdt_reset_n & rst_primary_n & fuse_reset_n
rst_wdt_n          = wdt_reset_n
```

Synchronised, zero-delay, to the output ports:

| Output                         | Equals            |
|--------------------------------|-------------------|
| `powergood_stable_o`           | `powergood_stable`|
| `rst_cold_stable_ref_clk_no`   | `stable_cold_rst_n` |
| `rst_cold_stable_smc_clk_no`   | `stable_cold_rst_n` |
| `rst_primary_ref_clk_no`       | `rst_primary_n`   |
| `rst_primary_smc_clk_no`       | `rst_primary_n`   |
| `rst_primary_periph_clk_no`    | `rst_primary_n`   |
| `rst_core_smc_clk_no`          | `rst_core_int_n`  |
| `rst_wdt_smc_clk_no`           | `rst_wdt_n`       |
| `rst_cool_no`                  | `cool_from_flr_n` (post-override) |

`smc_wdt_first_timeout_i` is accepted for interface fidelity but does **not**
participate in reset derivation (matches RTL).

---

## 5. JTAG overrides (`set_jtag_ctrl`)

Mirrors `smc_pkg::jtag_smc_reset_ctrl_t`.  When an `*_ovrd` flag is set, the
corresponding reset is forced to its `*_val`:

| Override                | Forces                                  |
|-------------------------|-----------------------------------------|
| `fuse_reset_n_ovrd`     | `fuse_reset_n` ← `fuse_reset_n_val`     |
| `cold_reset_n_ovrd`     | `cold_reset_n` ← `cold_reset_n_val`     |
| `cool_reset_n_ovrd`     | `cool_from_flr_n` / `rst_cool_no` ← `cool_reset_n_val` |
| `core_reset_n_ovrd`     | `rst_core_int_n` ← `core_reset_n_val`   |
| `ss_cold_reset_n_ovrd[i]` | `ss_reset_ctrl_o[i].cold_reset_n` ← `ss_cold_reset_n_val[i]` |
| `ss_warm_reset_n_ovrd[i]` | `ss_reset_ctrl_o[i].warm_reset_n` ← `ss_warm_reset_n_val[i]` |

---

## 6. Per-subsystem reset-control bundle (`reset_ctrl_t`)

For each subsystem `i ∈ [0, num_subsystems)`:

| Field                  | Source                                     |
|------------------------|--------------------------------------------|
| `cold_reset_n`         | `SS_COLD_RESET_N[i]` (JTAG-overridable)    |
| `warm_reset_n`         | `SS_WARM_RESET_N[i]` (JTAG-overridable)    |
| `config_state_hold`    | `SS_CONFIG_HOLD[i]`                        |
| `critical_signal_hold` | `SS_CRITICAL_HOLD[i]`                      |
| `sram_hold`            | `SS_SRAM_HOLD[i]`                          |
| `debug_hold`           | `SS_DEBUG_HOLD[i]`                         |
| `force_to_ref_clk_n`   | `SS_FORCE_TO_REF_CLK[i]`                   |

---

## 7. FLR cool-reset flow

On a rising edge of `cfg_flr_pf_active_i`:

1. `ISOLATE_REQ_SMC_REG.bit0` is set (HW). Software clears it by writing the
   register.
2. If `ISOLATE_REQ_FLR_RESET_COUNTER_VALUE != 0`, `rst_cool_no` is driven low
   after `ISOLATE_REQ_FLR_COUNTER_VALUE × ref_clk_period_ns` and released
   after a further `ISOLATE_REQ_FLR_RESET_COUNTER_VALUE × ref_clk_period_ns`.
   A zero reset-counter value suppresses the pulse entirely.

`skip_mem_repair_o = isolate_req_pin_i | ISOLATE_REQ_SMC_REG.bit0`.

`isolate_req_o[i] = ISOLATE_REQ_REG[i] | (ISOLATE_REQ_PINEN_REG[i] & isolate_req_pin_i) | (ISOLATE_REQ_SMCEN_REG[i] & ISOLATE_REQ_SMC_REG.bit0)`.

---

## 8. Reset domains for register state

| Domain                                | Trigger (low)               | Registers cleared |
|---------------------------------------|-----------------------------|-------------------|
| Primary (`rst_primary_smc_clk_no`)    | cold **or** cool/FLR reset  | `SS_CONFIG`, `SS_CONFIG_LOCK`, `SS_COLD_RESET_N`, `SS_WARM_RESET_N`→0xFFFFFFFF, `SS_*_HOLD`, `SS_COLD_RESET_LOCK`, `SS_FORCE_TO_REF_CLK`, `SYNC_REG` |
| Cold (`rst_cold_stable_smc_clk_no`)   | cold reset only             | `ISOLATE_REQ_REG`, `ISOLATE_REQ_SMC_REG`, `ISOLATE_REQ_SMCEN_REG`, FLR counters; `ISOLATE_REQ_PINEN_REG` only when the isolate pin is low |

Because the cold domain is **not** asserted by a cool/FLR reset, the FLR /
isolate registers retain their values across a cool reset.

---

## 9. TLM-2.0 error taxonomy

| Condition                                   | Response                          |
|---------------------------------------------|-----------------------------------|
| Access width ≠ 4 bytes                      | `TLM_BURST_ERROR_RESPONSE`        |
| Misaligned (addr & 0x3 ≠ 0)                 | `TLM_BURST_ERROR_RESPONSE`        |
| `streaming_width != data_length`            | `TLM_BURST_ERROR_RESPONSE`        |
| Byte-enable present                         | `TLM_BYTE_ENABLE_ERROR_RESPONSE`  |
| Address ≥ `WINDOW_SIZE`                     | `TLM_ADDRESS_ERROR_RESPONSE`      |
| Command not READ/WRITE                      | `TLM_COMMAND_ERROR_RESPONSE`      |
| Otherwise                                   | `TLM_OK_RESPONSE`                 |

DMI is never granted (register accesses have side effects).  `transport_dbg`
provides side-effect-free back-door access (no FLR latch / counter effects on
write decode beyond the register value).

---

## 10. Timing

A configurable annotated delay (`access_delay_ns`, default 2 ns) is added to
the `b_transport` delay argument for every register access.  FLR pulse timing
uses `ref_clk_period_ns` (default 10 ns).  All reset derivation is otherwise
combinational (zero-delay) in the LT model.
