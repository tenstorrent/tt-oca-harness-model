# SMC Bus Error Unit — Test Plan

> Verification strategy for the SystemC/TLM-2.0 LT model in
> `smc/peripherals/beu`. Benches: `test/beu_tb.cpp` (primary) and
> `test/beu_neg_tb.cpp` (negative / edge). Both self-check and print
> `ALL TESTS PASSED`.

## 1. Objectives

- Confirm the register map (offsets, masks, reset values) matches
  `bus_error_unit.rdl`.
- Confirm error recording semantics: accrued status, first-error latching into
  `CAUSE`/`PHYS_ADDR`, and recording-enable gating.
- Confirm dual interrupt delivery: local vs. PLIC lines, per-source masking, and
  software acknowledge / re-arm.
- Confirm robust bus behaviour: TLM response codes for malformed accesses.
- Achieve > 95 % line coverage of `src/beu.cpp` (achieved: 100 %).

## 2. Environment

- Accellera SystemC 2.3.x / 3.0.x + TLM-2.0, SystemC CCI 1.0, C++20.
- A tiny TLM initiator (`driver`) with a `tlm_quantumkeeper` issues 64-bit
  reads/writes and `transport_dbg` peeks; the DUT's interrupt outputs are wired
  to `sc_signal<bool>` observed by the bench.
- Error sources are driven through the `inject_error()` back door.

## 3. Test-bench architecture

| Bench | Focus |
|-------|-------|
| `beu_tb` | Functional happy-path: reset, register RW/RO/masking, error injection, latching/priority, gating, interrupts, CCI. |
| `beu_neg_tb` | Error / edge branches: TLM response codes, decode miss, misalignment, `transport_dbg` edges, RO write-ignore, gated injection. |

## 4. Primary test list (`beu_tb`)

| # | Test | Checks |
|---|------|--------|
| 1 | Reset values | `CAUSE=0`, `PHYS_ADDR=0`, `ENABLE=0xE6`, `PLIC_ENABLE=0`, `ACCRUED=0`, `LOCAL_ENABLE=0`, both IRQs low. |
| 2 | Register RW / RO / masking | `ENABLE`/`PLIC_ENABLE`/`LOCAL_ENABLE` mask reserved bits to `0xE6`; `CAUSE` keeps `[2:0]`; `PHYS_ADDR` write ignored. |
| 3 | Basic injection | `DCACHE_UNCORRECTABLE` sets `ACCRUED[7]`, `CAUSE=7`, `PHYS_ADDR=addr`. |
| 4 | First-error latch / re-arm | Second (`ICACHE_TLBUS`) error accrues but does not overwrite `CAUSE`/`PHYS_ADDR`; after `CAUSE=0` + `ACCRUED=0`, next error latches `CAUSE=1`. |
| 5 | Recording-enable gating | With `ENABLE=0`, an error still sets accrued status but `CAUSE`/`PHYS_ADDR` stay clear. |
| 6 | Local + PLIC aggregation | `LOCAL_ENABLE` mask raises `irq_local` only; adding `PLIC_ENABLE` also raises `irq_plic`; clearing `ACCRUED` drops both. |
| 7 | Per-source mask selectivity | A masked-out source does not raise `irq_plic`; an unmasked source does. |
| 8 | `transport_dbg` / `dbg_reg` peek | Peeks return values without disturbing state; unmapped offset peek returns 0. |
| 9 | CCI introspection | `access_delay_ns` preset visible + mutable via broker. |

## 5. Negative test list (`beu_neg_tb`)

| # | Test | Expected |
|---|------|----------|
| 1 | Ignore command | `TLM_COMMAND_ERROR_RESPONSE` |
| 2 | Wrong width (4 bytes) | `TLM_BURST_ERROR_RESPONSE` |
| 3 | Out-of-window (`0x1000`) | `TLM_ADDRESS_ERROR_RESPONSE` |
| 4 | Misaligned (`0x4`) | `TLM_ADDRESS_ERROR_RESPONSE` |
| 5 | In-window decode miss (`0x30`, read + write) | `TLM_ADDRESS_ERROR_RESPONSE` |
| 6 | Valid `ENABLE` read | `TLM_OK_RESPONSE` |
| 7 | RO `PHYS_ADDR` write | accepted (`OK`) but value unchanged (0) |
| 8 | `transport_dbg` bad length / misaligned / out-of-window / ignore cmd / unmapped write | returns 0 |
| 9 | `transport_dbg` valid read/write of `ENABLE` | returns 8; value updates |
| 10 | Gated injection | with `ENABLE=0`, `CAUSE` stays 0 while `ACCRUED` sets. |

## 6. Coverage

Run:

```bash
./run_tests.sh --coverage
```

`src/beu.cpp` reaches 100 % line coverage across the two benches. The primary
bench covers the functional lines and the `dbg_reg` peek fall-through; the
negative bench covers the four TLM error branches and the in-window decode-miss
path.

## 7. Pass / fail criteria

- Each bench increments a failure counter on any failed `EXPECT_*` and prints
  `ALL TESTS PASSED` iff the counter is zero (exit code 0).
- CTest matches `ALL TESTS PASSED` / `FAILURE(S)` via the regexes in
  `test/CMakeLists.txt`.
- CI regression is driven by `smc/run_all_smc_tests.sh beu` (Release, ASAN,
  Coverage, CTest stages).
