# SEP Software Reset Controller SystemC TLM High-Level Design Specification

**Document Version:** 1.0
**Date:** 2026-07-27
**IP Module:** SEP Software Reset Controller (`sep_reset_ctrl`)
**Modeling Approach:** SystemC TLM2.0, single register gating five downstream reset outputs

---

## Executive Summary

`sep_reset_ctrl` is a single 64-bit register (`SW_RESET_N`, only 5 bits meaningful) that ANDs firmware-controlled per-block reset bits with the global chip reset to drive five independent active-low reset outputs: KM, OTBN, AES, HMAC, KMAC. There is no address remap, filtering, or multi-entry table here — this is the smallest peripheral documented this session.

---

## 1. Introduction

Firmware uses this block to independently hold specific crypto accelerators in reset (e.g. during a key-wipe sequence) without resetting the whole SEP subsystem. Reset value `0x1E` (`0b11110`): bit 0 (`km_sw_rst_n`) is **0** — KM starts held in reset — while bits 1–4 (OTBN/AES/HMAC/KMAC) are **1** — released — matching the header comment's documented intent.

**Target audience:** SystemC model developers, VP integrators wiring the five reset outputs, firmware engineers sequencing crypto-block resets, verification engineers.

---

## 2. Features

### 2.1 Is (MUST Model)

- **`SW_RESET_N` register** (offset `0x0`, reset `0x1E`, mask `0x1F`): bits `[4:0]` = `km_sw_rst_n`, `otbn_sw_rst_n`, `aes_sw_rst_n`, `hmac_sw_rst_n`, `kmac_sw_rst_n`; bits `[63:5]` reserved.
- **AND with global reset**: each output is `global_rst_ni && sw_reset_bit` — asserting global reset always overrides, regardless of the SW register's value.
- **Delta-cycle-safe write handling** (§7.1) — the one genuinely non-obvious piece of logic in this peripheral.
- **Read-back reflects the live shadow value**, not raw CSML storage (`sw_reset_current_value_ & read_bit_mask`), kept in sync with every write via `handle_sw_reset_n_write()`.
- **Reset**: `reset_all_registers()` + `sw_reset_current_value_ = 0x1E` + re-notify outputs.

### 2.2 Is Not (EXCLUDE)

- No timing/latency on the reset outputs — asserted/deasserted within the same delta-cycle sequence described in §7.1, not modeling any real reset-synchronizer chain.
- No interaction with any other reset source (e.g. watchdog reset) — this block only reflects firmware's own `SW_RESET_N` writes ANDed with `global_rst_ni`.

---

## 3. Port Interfaces

| Port | Type | Direction | Description |
|------|------|-----------|--------------|
| `target_socket` (via base) | `tlm_utils::simple_target_socket<csml_memory<64>, 32>` | Target | CSR access, `0x8`-byte window. |
| `global_rst_ni` | `sc_in<bool>` | Input | Chip-level reset; ANDed into every output. |
| `km_rst_ni`, `otbn_rst_n`, `aes_rst_ni`, `hmac_rst_ni`, `kmac_rst_ni` | `sc_out<bool>` | Output | Per-block active-low reset, driven by `update_rst_outputs()`. |

Physical base address `0x10A5_0000` (per `Args.hpp`) is a VP integration detail, not encoded in the model.

---

## 4. Memory-Mapped Registers

| Register | Offset | Bits | Field | Reset | Access |
|----------|--------|------|-------|-------|--------|
| **SW_RESET_N** | 0x0 | [0] | `km_sw_rst_n` | 0 (held in reset) | RW |
| | | [1] | `otbn_sw_rst_n` | 1 (released) | RW |
| | | [2] | `aes_sw_rst_n` | 1 (released) | RW |
| | | [3] | `hmac_sw_rst_n` | 1 (released) | RW |
| | | [4] | `kmac_sw_rst_n` | 1 (released) | RW |
| | | [63:5] | Reserved | 0 | — |

---

## 5. Internal Architecture

```
sep_reset_ctrl_ip
├── sep_reset_ctrl_base       (SW_RESET_N register + target_socket)
├── global_rst_ni (in), km/otbn/aes/hmac/kmac_rst_ni (out)
└── (private) sw_reset_current_value_, sw_reset_changed_ (sc_event)

Write(SW_RESET_N, v):
  sw_reset_current_value_ = v & mask
  sw_reset_changed_.notify(SC_ZERO_TIME)
  wait(SC_ZERO_TIME)                          // see §7.1 — critical, not decorative
  return true

SC_METHOD update_rst_outputs [sensitive: global_rst_ni, sw_reset_changed_]:
  for each of the 5 outputs: out.write(global_rst_ni.read() && bit_set)
```

---

## 6. Modeling Assumptions

Reset sensitivity mirrors the other peripherals documented this session — a plain `sc_event`-driven `SC_METHOD`, not clock-synchronous, matching this VP's LT abstraction level throughout.

---

## 7. Fix-Worthy Detail Worth Calling Out

### 7.1 Why `handle_sw_reset_n_write()` Blocks on `wait(SC_ZERO_TIME)`

This is the one subtlety in an otherwise trivial peripheral, and the code comment explains it precisely: if firmware issues back-to-back assert-then-deassert writes to `SW_RESET_N` within the same simulation delta cycle (plausible from the ISS's `SC_THREAD`), both writes' `sw_reset_changed_.notify()` calls merge into a single event trigger — without the explicit `wait()`, `update_rst_outputs()` would only ever fire once, seeing the *final* (deasserted) value, so downstream peripherals would never actually observe the reset pulse. The `wait(sc_core::SC_ZERO_TIME)` forces the kernel to process `update_rst_outputs()` before `b_transport()` returns, guaranteeing the intermediate asserted state is actually visible to sensitive downstream `SC_METHOD`s.

---

## 8. Conclusion

A minimal, correct peripheral: one register, five gated outputs, one non-obvious delta-cycle fix. No known gaps.

---

**End of High-Level Design Specification**
