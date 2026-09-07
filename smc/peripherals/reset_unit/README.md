# SMC Reset Unit — SystemC / TLM-2.0 Loosely-Timed Model

A standards-compliant **SMC Reset Unit** modelled in Accellera SystemC
3.0.2 + TLM-2.0 (Loosely-Timed) and parameterised through SystemC
CCI 1.0.2.  It is the register-driven reset controller of the System
Management Controller (SMC): it sequences the chip-level cold / primary /
core / WDT resets, distributes per-clock-domain synchronised resets, drives
the 32 per-subsystem reset-control bundles, implements the PCIe
Function-Level-Reset (FLR) "cool reset" flow, and exposes a 32-bit AXI-Lite
register block to firmware.

The model tracks the RTL instantiated in the SMC sub-system:

- `tt-oca-harness/hw/smc/smc_reset_unit/data/registers/rdl/reset_unit.rdl` —
  the ground-truth register map.
- `tt-oca-harness/hw/smc/smc_reset_unit/rtl/smc_reset_unit.sv` — structural
  top wiring the four sub-blocks together.
- `tt-oca-harness/hw/smc/smc_reset_unit/rtl/smc_reset_ctrl.sv` — cold / primary /
  core / WDT reset derivation (de-glitch + extend counters).
- `tt-oca-harness/hw/smc/smc_reset_unit/rtl/smc_subsystem_resets.sv` —
  per-subsystem reset-control + lock logic.
- `tt-oca-harness/hw/smc/smc_reset_unit/rtl/smc_cool_reset_wrap.sv` — FLR
  cool-reset counters + isolate-request logic.
- `tt-oca-harness/hw/smc/smc_reset_unit/rtl/smc_reset_sync.sv` — per-clock-domain
  reset synchronisers.
- `tt-oca-harness/hw/smc/smc_pkg.sv` — `jtag_smc_reset_ctrl_t` JTAG override struct.
- `tt-oca-harness/hw/smc/data/registers/rdl/smc_top.rdl` — top-level map
  (`smc_reset_unit @ BASE_ADDR + 0x000_2000`).

Architecture, CSRs, and programming are in the hardware TRM. Model and
test docs:

- `doc/index.adoc` — entry (includes the two pages below)
- `doc/implementation.adoc` — SystemC/TLM model and `smc-vp` bind
- `doc/test_plan.adoc` — standalone cases and firmware tests

The model is a drop-in `SC_MODULE` that the rest of the SMC SystemC IP
library wires up exactly as for the Boot ROM, PLIC, CLINT, and Scratchpad RAM.

---

## Layout

```
reset_unit/
├── CMakeLists.txt
├── README.md                       (this file)
├── run_tests.sh                    Build + run helper (Release / ASan / ctest)
├── include/
│   ├── smc_tlm_extensions.h        Shared GP extension (smc_axi_extension)
│   └── reset_unit.h                SC_MODULE(reset_unit) declaration + cci_param
├── src/
│   └── reset_unit.cpp              Implementation
├── test/
│   ├── CMakeLists.txt
│   ├── reset_unit_tb.cpp           Primary self-checking bench
│   └── reset_unit_neg_tb.cpp       Negative-path / edge-case bench
└── doc/
    ├── index.adoc
    ├── implementation.adoc
    └── test_plan.adoc
```

---

## Module interface

```cpp
SC_MODULE(reset_unit) {
    tlm_utils::simple_target_socket<reset_unit> reg_socket; // AXI-Lite, 32-bit

    // Inputs
    sc_in<bool>     powergood_i, rst_cold_ni, fuse_reset_ni, rst_ext_wdt_ni;
    sc_in<bool>     smc_wdt_first_timeout_i, smc_wdt_second_timeout_i;
    sc_in<bool>     rst_cool_ni, isolate_req_pin_i, cfg_flr_pf_active_i;
    sc_in<uint32_t> ss_reset_complete_i;
    sc_in<uint64_t> captured_straps_i;

    // Outputs
    sc_out<bool>     powergood_stable_o;
    sc_out<bool>     rst_cold_stable_ref_clk_no, rst_cold_stable_smc_clk_no;
    sc_out<bool>     rst_primary_ref_clk_no, rst_primary_smc_clk_no, rst_primary_periph_clk_no;
    sc_out<bool>     rst_core_smc_clk_no, rst_wdt_smc_clk_no;
    sc_out<bool>     rst_cool_no, skip_mem_repair_o, sync_irq_o;
    sc_out<uint32_t> isolate_req_o, ss_config_o;
    sc_vector<sc_out<reset_ctrl_t>> ss_reset_ctrl_o;        // per-subsystem bundle

    explicit reset_unit(sc_module_name, reset_unit_cfg = reset_unit_cfg{});
    void set_jtag_ctrl(const jtag_reset_ctrl&);            // JTAG overrides
};
```

`reset_unit_cfg` defaults:

| Field               | Default | Note                                                       |
|---------------------|---------|------------------------------------------------------------|
| `num_subsystems`    | `32`    | Sizes `ss_reset_ctrl_o[]` (reg fields are 32-bit).         |
| `ref_clk_period_ns` | `10.0`  | Reference-clock period used for FLR cool-reset timing.     |
| `access_delay_ns`   | `2.0`   | TLM `b_transport` annotated delay (AXI-Lite latency).      |

All three are exposed as CCI parameters (`access_delay_ns` mutable, the rest
immutable).  Set them via the broker before constructing:

```cpp
broker.set_preset_cci_value("smc.reset_unit.ref_clk_period_ns", cci::cci_value(10.0));
broker.set_preset_cci_value("smc.reset_unit.access_delay_ns",   cci::cci_value(2.0));
```

See `doc/implementation.adoc` for the CCI catalogue and `smc-vp` bind.

---

## Behaviour highlights

- **Register block**: 32-bit AXI-Lite-style, 8-bit address window
  (`reset_unit.rdl`, `RESET_UNIT_REG_SIZE = 0xCC`).  Locks (`SS_CONFIG_LOCK`,
  `SS_COLD_RESET_LOCK`) are write-1-to-set sticky and write-protect the
  matching `SS_CONFIG` / `SS_COLD_RESET_N` bits.
- **Reset derivation** (functional abstraction of `smc_reset_ctrl.sv`):
  ```
  stable_cold_rst_n = powergood & cold_reset_n
  rst_primary_n     = stable_cold_rst_n & rst_cool_ni & cool_from_flr_n
  rst_core_n        = (rst_ext_wdt_ni & ~wdt_2nd_timeout) & rst_primary_n & fuse_reset_n
  rst_wdt_n         = rst_ext_wdt_ni & ~wdt_2nd_timeout
  ```
  The 32-cycle cold-reset de-glitch, 255-cycle extender, and powergood stretch
  are abstracted to zero latency (functional/firmware modelling, not CDC).
- **Per-clock-domain synchronisers** (`smc_reset_sync.sv`) are zero-delay
  pass-through, e.g. `rst_primary_smc_clk_no == rst_primary_n`.
- **Per-subsystem bundles**: `SS_COLD_RESET_N`, `SS_WARM_RESET_N`,
  `SS_*_HOLD`, `SS_FORCE_TO_REF_CLK` map bit `i` to `ss_reset_ctrl_o[i]`.
- **JTAG overrides** (`set_jtag_ctrl`): force fuse / cold / cool / core resets
  and per-subsystem cold/warm resets, mirroring `jtag_smc_reset_ctrl_t`.
- **FLR cool-reset flow**: a rising edge on `cfg_flr_pf_active_i` sets
  `ISOLATE_REQ_SMC_REG.bit0` (software clears it) and — if the reset-duration
  counter is non-zero — pulses `rst_cool_no` low after
  `FLR_COUNTER_VALUE × ref_clk_period_ns`, for
  `FLR_RESET_COUNTER_VALUE × ref_clk_period_ns`.
- **Isolate request**:
  `isolate_req_o[i] = ISOLATE_REQ_REG[i] | (ISOLATE_REQ_PINEN_REG[i] & pin) | (ISOLATE_REQ_SMCEN_REG[i] & smc_latch)`.
- **Reset domains**: a cold reset clears every register; a cool/FLR reset
  asserts only the primary domain, so the FLR / isolate registers (cold
  domain) retain their values — matching the RTL.

### Unmodelled RTL details (documented simplifications)

- **CDC / metastability** — the four-deep `prim_sync_reset` /
  `prim_sync_data_autohs` synchronisers are zero-delay pass-through.
- **De-glitch / extend / stretch counters** — abstracted to zero latency
  (the 320 ns cold-reset de-glitch and 255-cycle extender do not affect
  functional behaviour).
- **Scan / test_mode / scan_rst_n** — DFT-only; not modelled.

---

## Build & test

```bash
./run_tests.sh                   # Release build + run both test benches
./run_tests.sh --ctest           # Run via ctest (both binaries)
./run_tests.sh --asan            # AddressSanitizer (+ LSan on Linux)
./run_tests.sh --coverage        # ≥ 95% line on src/; do not combine with --asan
./run_tests.sh --clean           # Wipe build/ first
```

This repository mandates **C++20** (the local SystemC / CCI installs are
built `-std=c++20`).  Point the helper at the C++20 toolchain:

```bash
SYSTEMC_HOME=/path/to/systemc-3.0.2-cxx20 \
CCI_HOME=/path/to/cci-cxx20 \
./run_tests.sh --clean
```

`SYSTEMC_HOME` and `CCI_HOME` are otherwise auto-probed for the common
install locations. There is no `smc-reset-test` firmware directory;
see `doc/test_plan.adoc`.

---

## Current status

| Metric                       | Value                          |
|------------------------------|--------------------------------|
| Test cases (across two TBs)  | All PASS (ctest: 2/2)          |
| AddressSanitizer             | 0 errors                       |
| C++ standard                 | C++20 (repo toolchain)         |

---

## Citation

Modelled after, and consistent with, the SMC Boot ROM, PLIC, CLINT, and
Scratchpad RAM SystemC sub-packages in `smc/peripherals/`.  See those READMEs
and the matching specification / low-level-design documents for the shared SMC
IP conventions (CCI declaration order, single-driver discipline, error
taxonomy, build & packaging).
