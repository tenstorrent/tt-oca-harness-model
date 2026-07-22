# SMC CPU Control — Low-Level Design

**Document**: `02_CPU_CTRL_LowLevel_Design.md`  
**Module**: `smc::cpu_ctrl`  

---

## 1. Architecture

```
                    ┌─────────────────────────────────────┐
  TLM initiator ──► │ reg_socket (simple_target_socket)   │
  (fabric / TB)     │   b_transport / transport_dbg       │
                    │           │                         │
                    │           ▼                         │
                    │  normalize_addr(base_addr)          │
                    │           │                         │
                    │           ▼                         │
                    │  reg_read / reg_write               │
                    │  (64-bit aligned merge, sub-width)  │
                    │           │                         │
                    │           ▼                         │
                    │  storage: reset_vector_, scratch_,  │
                    │  mutex_, sema_, …                   │
                    └─────────────────────────────────────┘
```

The module is **purely LT**: no `wait()` in `b_transport`, no quantum keeper
inside the target.  Temporal decoupling is the initiator's responsibility.

---

## 2. Source layout

```
cpu_ctrl/
├── include/cpu_ctrl.h          SC_MODULE + handoff constants
├── src/cpu_ctrl.cpp            Register decode + TLM callbacks
├── test/cpu_ctrl_tb.cpp        Self-checking bench
└── doc/                        Specification + test plan
```

`smc_axi_extension` / `source_id_t` are pulled from the shared canonical
header `smc/common/include/smc_axi_extension.h` (added to the include path
by this target's CMake); the peripheral no longer carries a local copy.

---

## 3. Register storage

All RDL registers are modeled as `uint64_t` (or `uint16_t` for SEMA counters)
with power-on values taken from `cpu_ctrl.rdl`.  SCRATCH entries mask to
32 bits on write (`data[31:0]`).

---

## 4. Address normalization

```cpp
if (addr >= base_addr && addr < base_addr + WINDOW_SIZE)
    off = addr - base_addr;
else
    off = addr;  // standalone bench uses offsets
```

This allows binding to `smc_fabric.to_cpu_ctrl` (full system addresses) and
direct unit tests at offset zero.

---

## 5. Fabric integration

In a full platform netlist:

```cpp
fabric.to_cpu_ctrl.bind(cpu_ctrl_dut.reg_socket);
```

Preset `cpu_ctrl.base_addr` to `0xC0010000`.  Note that `smc_fabric` may
handle `GLOBAL_BASE` / `LOCAL_BASE` / `REGION_SIZE` internally when the
transaction enters through the fabric MMIO port; the cpu_ctrl copies remain
consistent for direct `to_cpu_ctrl` probes.

---

## 6. HW backdoor API

For strap-driven read-only registers:

| Method | Register |
|--------|----------|
| `set_smc_attributes(v)` | `SMC_ATTRIBUTES @ 0x1000` |
| `set_test_ctrl(v)` | `TEST_CTRL @ 0x200` |
| `set_wb_pc(core, slot, pc)` | `WB_PC_COREn[slot]` |

Handoff helpers: `scratch(idx)`, `set_scratch(idx, v)`, `dump_state(os)`.

---

## 7. Debug transport

`transport_dbg` performs **side-effect-free** reads (mutex acquire is **not**
triggered).  Used by platform debuggers and the test bench mutex peek case.

---

## 8. Deliberate simplifications

| RTL feature | LT model |
|-------------|----------|
| Core reset pulse FSM | Pulse bits self-clear; no `sc_out` reset nets |
| `RESET_CTRL` external | Storage only |
| Clock gate enables | Storage only; no clocking side effects |
| `SEMA` external hardware | In-module 16-bit counter |
| Strap → `SMC_ATTRIBUTES` | Backdoor / default zero until platform drives it |

These simplifications keep the model focused on **firmware-visible CSR
behaviour** and **SMC↔SEP scratch handoff**, which is the bring-up critical path.
