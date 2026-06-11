# SMC Reset Unit — Test Plan

Verification plan for the SMC Reset Unit SystemC/TLM-2.0 LT model.  Two
self-checking benches (`test/reset_unit_tb.cpp`, `test/reset_unit_neg_tb.cpp`)
print `ALL TESTS PASSED` on success and exit non-zero on any failure; both are
registered with CTest.

---

## 1. Strategy

- **Functional bench** (`reset_unit_tb`) wires the DUT into a full SystemC
  elaboration (all ports bound to signals, a TLM initiator on the register
  socket) and drives stimulus from an `SC_THREAD`, checking register
  read-backs, derived reset outputs, the per-subsystem bundles, and the FLR
  pulse timing.
- **Negative bench** (`reset_unit_neg_tb`) probes constructor guard rails
  (which `SC_REPORT_FATAL`/throw) and the TLM error taxonomy.  It runs entirely
  during elaboration (direct `b_transport` calls, no `sc_start`) so the
  constructor-throw probes cannot perturb a running kernel — the same pattern
  the sister-IP negative benches use.
- **AddressSanitizer** is run via `run_tests.sh --asan` to confirm no memory
  errors.

---

## 2. Functional bench coverage (`reset_unit_tb`)

| # | Test | Checks |
|---|------|--------|
| 1 | Out-of-reset derivation | All derived resets de-asserted (high) once pins released. |
| 2 | RW register access | Reset defaults (`SS_WARM_RESET_N = 0xFFFFFFFF`); write/read of `SS_WARM_RESET_N`, `SS_CONFIG_HOLD`, `SS_DEBUG_HOLD`. |
| 3 | Lock semantics | `SS_CONFIG_LOCK` woset (sticky, accumulating); locked `SS_CONFIG` bits write-protected. |
| 4 | Per-subsystem bundle | `SS_COLD_RESET_N` / `SS_WARM_RESET_N` / `SS_SRAM_HOLD` / `SS_FORCE_TO_REF_CLK` bits map to `ss_reset_ctrl_o[i]` and `dbg_ss_reset_ctrl(i)`. |
| 5 | `ss_config_o` | `SS_CONFIG` value reflected on the output port. |
| 6 | Straps | `STRAPS_LO` / `STRAPS_HI` reflect `captured_straps_i`. |
| 7 | Cold reset | Asserts cold/primary/core; clears all registers; releases to defaults. |
| 8 | WDT 2nd timeout | Forces core + WDT reset, leaves primary up. |
| 9 | Fuse reset | Gates core reset only. |
| 10 | JTAG overrides | Forced cold reset; per-subsystem warm-reset override; clear restores. |
| 11 | Isolate logic | `ISOLATE_REQ_REG` + pin-enable (`ISOLATE_REQ_PINEN_REG` & pin); `skip_mem_repair_o`; `ISOLATE_REQ_VIS`. |
| 12 | FLR cool-reset | FLR rising edge sets `ISOLATE_REQ_SMC_REG`, asserts smc-enabled isolate, `skip_mem_repair_o`; `rst_cool_no` pulses low after the delay and releases after the hold; SW write clears the latch. |
| 13 | Reset domains | Cool (from-pin) reset clears primary-domain regs but **retains** FLR/isolate (cold-domain) regs. |
| 14 | `SYNC_REG` | Drives `sync_irq_o`. |
| 15 | `transport_dbg` | Back-door write/read round-trip. |
| 16 | CCI | Param discovery; mutate `access_delay_ns`; `num_subsystems` immutability. |
| 17 | `dump_state` | Snapshot contains expected fields. |

---

## 3. Negative bench coverage (`reset_unit_neg_tb`)

| # | Test | Checks |
|---|------|--------|
| 1 | Constructor guard rails | `num_subsystems = 0` and `= 33`, `ref_clk_period_ns < 0`, `access_delay_ns < 0` each raise `SC_REPORT_FATAL`. |
| 2 | TLM error taxonomy | Non-word width, misaligned, streaming-width mismatch, byte-enable, out-of-window, bad command → the canonical responses (spec §9). |
| 3 | RAZ/WI hole | A read/write of an unmapped offset returns `TLM_OK_RESPONSE` with zero data. |
| 4 | `transport_dbg` | Malformed accesses return 0; a valid write/read round-trips. |

---

## 4. Running

```bash
# C++20 toolchain (repo standard)
SYSTEMC_HOME=/Users/pdroy/local/systemc-3.0.2-cxx20 \
CCI_HOME=/Users/pdroy/local/cci-cxx20 \
./run_tests.sh --clean        # build + run both benches

./run_tests.sh --ctest        # via CTest (2/2)
./run_tests.sh --asan         # AddressSanitizer
```

---

## 5. Results (reference run)

| Metric                | Result            |
|-----------------------|-------------------|
| `reset_unit_tb`       | ALL TESTS PASSED (17 groups) |
| `reset_unit_neg_tb`   | ALL TESTS PASSED  |
| CTest                 | 2/2 passed        |
| AddressSanitizer      | 0 errors          |

---

## 6. Gaps / future tests

- Cycle-accurate de-glitch / extend / stretch timing (currently abstracted).
- Register write-gating while a reset is asserted (currently permitted).
- Multi-subsystem FLR / isolate sweeps and randomised register soak.
