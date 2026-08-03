# SMC Watchdog Timer (WDT) — SystemC / TLM-2.0 LT Model

Loosely-timed SystemC model of one **SiFive / Chipyard TLWDT** (stage 1)
as used by the SMC CPU cluster. Absolute bases:

`0xC000_0000 + N×0x400` (1 KiB per core).

Stage-2 countdown (`WDT_TIMEOUT` / `WDT_TIMEOUT_RESET`) lives in
`cpu_ctrl` / `smc_cpu_cluster`, not in this IP.

See **`doc/00_Integration_and_Placement.md`** for fabric decode, RTL signal
mapping, and VP binding notes.

## Layout

```
wdt/
├── CMakeLists.txt
├── README.md
├── run_tests.sh
├── include/wdt.h
├── src/wdt.cpp
├── test/wdt_tb.cpp
└── doc/
```

## Ports

| Port | Dir | Role |
|------|-----|------|
| `reg_socket` | target | MMIO (KEY/FEED/CTRL/…) |
| `rst_n_i` | in | Active-low module reset |
| `core_rst_i` | in | Core-in-reset (active-high) for `wdogcoreawake` |
| `irq_o` | out | Level IRQ (`wdogip0`) → PLIC |
| `rst_sticky_o` | out | Sticky rst → stage-2 / `wdt_first_timeout` OR |

## Build & test

```bash
export SYSTEMC_HOME=…   # if needed
export CCI_HOME=…       # if needed
./run_tests.sh
```

## References

- `hw/smc/smc_cpu/data/registers/rdl/wdt.rdl`
- `OCAH*WatchdogTimer.sv` / `OCAH*TLWDT.sv`
- `smc/doc/systemc_tlm2_integration_guide.adoc`
