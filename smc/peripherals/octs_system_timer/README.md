# `octs_system_timer` — OCTS System Timer model

Architecture, CSRs, and programming sequences are in the hardware TRM.
This tree has the model, its test plan, and how to run the tests.

- `doc/index.adoc` — landing page
- `doc/implementation.adoc` — sockets, ports, processes, CCI, `smc-vp` bind
- `doc/test_plan.adoc` — standalone cases and firmware tests

A cycle-accurate SystemC/TLM-2.0 model of the **OCTS (Open Chiplet Time
Synchronization) System Timer**: a 64-bit timer that keeps a nanosecond-level
timeline coherent across chiplets using a PRIMARY/SECONDARY hierarchy with a
credit-based synchronization protocol.

> This IP can be built **standalone** (as `libsmc_octs_system_timer.a`) and ships a
> self-checking unit testbench. It is also integrated into `vp/platform/smc` (see
> "SMC platform integration" below).

## Provenance

The model is transcribed from the hardware sources, not from prose:

| Source (under `sw/tt-oca-harness-main/hw/ip/system_timer_octs/`) | Used for |
|---|---|
| `data/registers/rdl/system_timer_octs.rdl` | Register map, field masks, reset values |
| `rtl/system_timer_octs_core.sv` | Datapath, credit accounting, pulse FSM |
| `rtl/templates/system_timer_octs.sv.tpl` | CSR→core wiring (`CTRL.STEP`), CREDIT_EXPIRED peak tracker |
| `hw/common/prim/rtl/prim_edge_detector.sv` | Input synchronizer depth and edge-pulse semantics |
| `doc/{architecture,interface,memmap}.adoc` | Behavioural spec, programming sequence, access rules |

The same material is in `sw/tt-oca-harness-main/doc/dist/ocah-documentation.pdf`.

## Register map

Window size **0x24 (36 bytes)**. Per `memmap.adoc` **only naturally aligned
32-bit accesses are supported** — byte, halfword and misaligned accesses return
`TLM_BURST_ERROR_RESPONSE`, and anything at or past `0x24` returns
`TLM_ADDRESS_ERROR_RESPONSE`. The 64-bit preset and count are therefore split
into adjacent LO/HI register pairs.

| Off | Name | SW | Reset | Notes |
|-----|------|----|-------|-------|
| 0x00 | `TIMER_START` | RW (singlepulse) | 0 | Writing `START[0]=1` emits a one-cycle start pulse. Always reads 0. |
| 0x04 | `CTRL` | RW | `0x0001020A` | `CREDIT_VAL[7:0]`, `PULSE_WIDTH[15:8]`, `STEP[23:16]`; `[31:24]` reserved (RAZ/WI). |
| 0x08 | `STATUS` | RO | hw | `MODE[0]` (0=PRIMARY, 1=SECONDARY), `RUNNING[4]` (`enable && count > 0`). |
| 0x0C | `TIMER_PRESET_LO` | RW | 0 | Preset `[31:0]`. |
| 0x10 | `TIMER_PRESET_HI` | RW | 0 | Preset `[63:32]`. |
| 0x14 | `TIMER_COUNT_LO` | RO | hw | Live count `[31:0]`. |
| 0x18 | `TIMER_COUNT_HI` | RO | hw | Live count `[63:32]`. |
| 0x1C | `CREDIT_EXPIRED` | R, write-to-clear | 0 | Peak consecutive cycles the credit budget stayed exhausted. Writing *any* value clears it. |
| 0x20 | `TIMER_GPIO_ENABLE` | RW | 0 | `GPIO_ENABLE[0]` → `timer_gpio_enable_o`. |

## Why this model is clock-driven

Most register-file peripherals in this tree are loosely timed. Here the IP's
entire *function* is its per-cycle behaviour — counter step, credit
accounting, programmable pulse widths, clock-domain-crossing latency — so the
model takes an explicit `clk_i` and evaluates **one RTL clock cycle per rising
edge**, computing all next-state values from the current state before
committing them together. Pulse widths, credit periods and synchronizer
latency are consequently directly observable and regression-testable.

A user just binds an `sc_clock`; there is no free-running internal tick.

### Ports

| Port | Dir | Width | Description |
|------|-----|-------|-------------|
| `reg_socket` | tgt | — | TLM-2.0 target socket (32-bit register access) |
| `clk_i` | in | 1 | Clock |
| `rst_n_i` | in | 1 | Active-low reset, asynchronous assertion |
| `is_primary_i` | in | 1 | 1 = PRIMARY, 0 = SECONDARY (run-time, as in the RTL) |
| `timer_sync_load_i` | in | 1 | Sync-load pulse in (SECONDARY) |
| `timer_cnt_credit_i` | in | 1 | Credit pulse in (SECONDARY) |
| `timer_sync_load_o` | out | 1 | Sync-load pulse out (PRIMARY) |
| `timer_cnt_credit_o` | out | 1 | Credit pulse out (PRIMARY) |
| `timer_count_o` | out | 64 | Live counter |
| `timer_gpio_enable_o` | out | 1 | `TIMER_GPIO_ENABLE.GPIO_ENABLE` |
| `cur_credits_debug_o` | out | 9 | Credit accumulator (debug) |
| `credits_left_debug_o` | out | 1 | `cur_credits < CREDIT_VAL` (debug) |

## Behaviour

### PRIMARY (`is_primary_i = 1`)

Writing `TIMER_START` latches `enable`, loads the counter from the 64-bit
preset, and starts the pulse generator. The counter then increments by **1**
every cycle (a PRIMARY ignores `CTRL.STEP`). An 8-bit credit generator counts
`0 … CREDIT_VAL-1` and emits a credit pulse on wrap, so credit pulses have a
period of exactly `CREDIT_VAL` cycles. Both `sync_load_o` and `cnt_credit_o`
are asserted for `PULSE_WIDTH` cycles (`PULSE_WIDTH = 0` behaves as 1).
`sync_load_o` is a one-shot per START. Sync *inputs* are ignored.

### SECONDARY (`is_primary_i = 0`)

Idle until a `sync_load` pulse arrives, which loads the preset, anchors
`expected_count`, and latches `enable`. The counter then advances by
`CTRL.STEP` each cycle while `cur_credits < CREDIT_VAL`, accumulating
`cur_credits` by `STEP`. Once the budget is exhausted the counter **halts** and
an expiry counter ticks every starved cycle. A credit pulse resets the budget
and re-anchors the counter to `expected_count + CREDIT_VAL` — which *pulls the
counter back* if `STEP` overshot the PRIMARY's timeline. Sync *outputs* stay
low.

### Input synchronizer latency

`timer_sync_load_i` and `timer_cnt_credit_i` are asynchronous. The RTL path is
one input flop, then `prim_edge_detector` (a 2-flop synchronizer plus one
edge-detect flop), so a rising edge is consumed by the datapath
`octs_system_timer::SYNC_LATENCY_CYCLES` (= 4) rising edges later. This is
modelled exactly, so a SECONDARY's count trails its PRIMARY by a **constant 4
cycles**, and the credit protocol keeps re-anchoring it to that offset.

### Two faithful-to-RTL quirks worth knowing

- **A SECONDARY's credit accounting is gated only on `~is_primary_i`, not on
  the timer being started.** An un-synced SECONDARY therefore drains
  `cur_credits` and piles up a meaningless `CREDIT_EXPIRED`. That is precisely
  why `memmap.adoc` has software clear the register; the model reproduces it
  rather than "fixing" it.
- **`CREDIT_EXPIRED` is a peak tracker that lags its source by one cycle** (the
  top-level wrapper latches `max(peak, credit_expired)`), and a write clears
  only the peak — if the timer is *currently* starved the peak immediately
  re-latches the ongoing run.

`CREDIT_VAL` must be non-zero and greater than `PULSE_WIDTH`. The model emits
one `SC_WARNING` if software programs an illegal combination, standing in for
the RTL's `CreditValGreaterThanPulseWidth_A` assertion.

## Programming sequence

From `memmap.adoc` — note that **every** participating timer needs its own
preset, because a SECONDARY latches its own `TIMER_PRESET_*` when the
`sync_load` pulse arrives:

1. Program `CTRL` (`CREDIT_VAL`, `PULSE_WIDTH`, `STEP`) on all timers.
2. Program `TIMER_PRESET_LO` / `TIMER_PRESET_HI` to the **same** value on all
   timers.
3. Write `TIMER_START = 1` on the PRIMARY.
4. Optionally clear `CREDIT_EXPIRED`, then poll it to monitor sync health.

Reading the 64-bit count needs the usual two-register dance (`LO`, `HI`, re-read
`LO`; retry if `LO` wrapped).

## CCI parameters

| Name | Type | Default | Mutability | Purpose |
|------|------|---------|------------|---------|
| `access_delay_ns` | `double` | 2.0 | mutable | Annotated `b_transport` delay. |

## SMC platform integration

Wired into `smc-vp` (`vp/platform/smc/`) at **0xC000_A000**, the base
`smc_top.rdl` assigns to `system_timer_octs` (it owns the whole 0x1000
peripheral slot; offsets above the 0x24 register window fault). The window hangs
off `periph_router`, behind the fabric's `to_periph` port.

Two things are specific to this IP:

- **It needs a clock.** Every other peripheral on that bus is loosely timed, so
  the platform instantiates an `sc_clock` (`octs_clk`) just for this model. The
  period comes from the platform's `octs_clk_period_ns` CCI parameter, default
  10 ns (100 MHz). That resolution matters because the LT CPU cluster charges a
  notional 1 ns per retired instruction, so a firmware busy-wait of a few
  hundred instructions has to span enough timer cycles for the count to move
  visibly. Cost is negligible — a firmware test spans microseconds of simulated
  time, i.e. a few thousand ticks.
- **Mode is a strap.** `is_primary_i` is driven from the platform's
  `octs_is_primary` CCI parameter, default `true`: SMC is the system
  timekeeping PRIMARY, so it *sources* `sync_load` / `cnt_credit` and its sync
  inputs are tied low. Nothing in the SMC VP consumes the outputs yet, so they
  land on sinks.

Firmware tests live in `sw/smc-vp-tests/` and mirror the RTL firmware OCTS tests
in `tt-oca-harness-main/fw/smc/tests`:

| Test | Covers |
|------|--------|
| `smc-octs-timer-test/` | PRIMARY datapath and the full register contract (mirrors `octs_p0_primary_test` + the primary half of `octs_p0_credit_test`) |
| `smc-octs-timer-secondary-test/` | `octs_is_primary=false` via its own `.ini`: STATUS.MODE reads SECONDARY and the counter stays parked with no `sync_load` source (mirrors `octs_p0_sec_test`) |

The end-to-end PRIMARY → SECONDARY sync path (sync_load, credit budget,
starvation, re-anchor) needs two timers and is covered by the unit testbench
below, not from firmware.

## Building and testing

```bash
cp deps.env.example deps.env      # point SYSTEMC_HOME / CCI_HOME at your installs
./run_tests.sh                    # incremental build + run   (Release)
./run_tests.sh --clean            # wipe build/ and reconfigure
./run_tests.sh --ctest            # run through ctest
./run_tests.sh --asan             # AddressSanitizer build + report
./run_tests.sh --coverage         # instrumented build + line-coverage report
```

Requires SystemC 3.0.2 and CCI 1.0.2 (the same versions as the other SMC
peripheral models).

### What the testbench covers

`test/octs_system_timer_tb.cpp` instantiates three DUTs on one clock: a
PRIMARY, a SECONDARY wired to it (the end-to-end protocol check), and a
standalone SECONDARY whose sync inputs the TB drives by hand (so starvation and
re-anchoring can be provoked deterministically). Stimulus runs on the clock's
falling edge, halfway between the edges the DUT evaluates on, which is what
makes the exact cycle counts in the checks meaningful.

15 groups / 145 checks: reset defaults; `STATUS.MODE`; RW/RO/reserved-bit
contracts; access rules and annotated delay; PRIMARY start and 64-bit preset
load; `sync_load` pulse width; credit pulse width and period; PRIMARY→SECONDARY
constant-lag tracking; idle-SECONDARY behaviour; sync latency, load value and
`STEP` counting; credit starvation and `CREDIT_EXPIRED` accumulation; replenish
and write-to-clear; credit re-anchor pulling an over-run counter back; the
`CREDIT_VAL`/`PULSE_WIDTH` constraint warning; CCI preset, discovery and live
mutation.
