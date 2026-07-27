# WDT — RTL placement and SystemC integration

## Two-stage watchdog in SMC RTL

| Stage | RTL location | Programmer view | Role |
|-------|----------------|-----------------|------|
| **1** | Chipyard `TLWDT` / `WatchdogTimer` inside `smc_cpu` cluster | `wdt.rdl` @ `0xC000_0000 + N×0x400` | Per-core counter, KEY/FEED, compare IRQ, optional sticky `rst` |
| **2** | `smc_cpu_ctrl_wrap.sv` (`cpu_ctrl` CSRs) | `WDT_TIMEOUT` @ `0x50`, `WDT_TIMEOUT_RESET` @ `0x58` (window-relative) | Counts SMC-clock cycles while stage-1 sticky is asserted; second expiry → `wdt_second_timeout_o` → reset unit |

Stage **1** is what `smc::wdt` models (one `sc_module` instance per core).

Stage **2** is modeled in `smc_cpu_cluster` / `cpu_ctrl` (not inside `peripherals/wdt`).

## Where the hardware WDT sits

```
CPU MMIO (local crossbar, LOCAL_BASE = 0xC000_0000)
│
├── 0xC000_0000 .. 0xC000_0FFF  ──► smc_fabric.to_front_port  ("WDT / debug")
│       │
│       ├── Core 0 WDT  0xC000_0000 .. 0xC000_03FF  (1 KiB)
│       ├── Core 1 WDT  0xC000_0400 .. 0xC000_07FF
│       ├── Core 2 WDT  0xC000_0800 .. 0xC000_0BFF
│       └── Core 3 WDT  0xC000_0C00 .. 0xC000_0FFF
│
├── 0xC004_0000 ..           scratchpad, PLIC, CLINT, … (also front_port)
└── 0xC000_2000 ..           cpu_ctrl, UART, … (to_periph)
```

Inside RTL, each `TLWDT` runs on the cluster **uncore / peripheral bus clock** (`clock` on
`OCAH*Cluster_TLWDT`), not a separate AON RTC. The counter advances once per clock when
enabled (`wdogenalways` or `wdogcoreawake` && core not in reset). `scale` divides the
compare path (`scaled = count >> wdogscale`).

### Sideband signals (stage 1 → SoC)

| RTL signal | Direction | SystemC port | Downstream |
|------------|-----------|--------------|------------|
| `wdt_N_rst` / `scale_io_rst` | out | `rst_sticky_o` | `wdt_timeout_cluster_i[N]` → stage-2 in `cpu_ctrl_wrap` |
| WDT interrupt | out | `irq_o` | PLIC source (per core) |
| `wdt_N_corerst` | in | `core_rst_i` | Core reset (active-high “in reset”) for awake mode |
| Cluster reset | in | `rst_n_i` | Module reset |

`wdt_first_timeout_o` at `smc` top is `|cpu_wdt_timeout_cluster_i` (any core stage-1 sticky).
`wdt_second_timeout_o` comes from stage-2 only and drives `smc_reset_ctrl` with
`rst_ext_wdt_ni`.

## SystemC directory and binding (target picture)

```
repo/tt-oca-sim/smc/
├── smc_fabric/          decode 0xC000_0000–0xFFF → to_front_port
├── cpu_cluster/         optional front-port demux + stage-2 WDT logic
└── peripherals/wdt/     smc::wdt × NUM_CPU_CORES
```

**Recommended VP wiring (not yet in platform):**

1. Front-port address router (or cluster wrapper) subtracts instance base:
   `local_off = addr - (0xC000_0000 + core_id * 0x400)`.
2. Bind `wdt[core].reg_socket` to that router’s target socket.
3. Tie `wdt[core].core_rst_i` to the core’s reset status.
4. Tie `wdt[core].rst_n_i` to cluster uncore reset.
5. OR `wdt[*].rst_sticky_o` → `cpu_cluster.wdt_timeout_cluster_i[*]`.
6. Route `wdt[*].irq_o` into the PLIC `src_in` map (firmware uses
   `metal_watchdog_get_interrupt_id`). Note: **SEP external WDT** is a
   different IRQ (1-core PLIC ID 59 / 4-core PLIC ID 283) and is **not**
   this Chipyard TLWDT model.

## Timing model (no `sc_clock`)

This IP uses **LT timed events**, same pattern as `peripherals/clint`:

- CCI `tick_period_ns` (default 100 ns) → periodic `tick_event_` → increment `count_`.
- Set `tick_period_ns = 0` for test-controlled time (`dbg_tick`).
- CCI `access_delay_ns` annotates TLM `b_transport` only.

There is **no** `sc_clock`, **no** SCML clock, and **no** explicit `clk_smc_i` port;
map RTL WDT clock frequency by choosing `tick_period_ns` when integrating the VP.

## Files in this package

| File | Purpose |
|------|---------|
| `include/wdt.h` | Register map, ports, `smc::wdt` class |
| `src/wdt.cpp` | LT behaviour |
| `test/wdt_tb.cpp` | Unit tests (not run until you invoke `run_tests.sh`) |
| `doc/01_WDT_Specification.md` | Register / behaviour summary |
| `doc/02_WDT_LowLevel_Design.md` | SC_METHOD / event structure |
