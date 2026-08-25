# SMC Watchdog Timer (WDT) — SystemC / TLM-2.0 LT Model

Loosely-timed SystemC model of one **SiFive / Chipyard TLWDT** (stage 1)
as used by the SMC CPU cluster. Absolute bases:

`0xC000_0000 + N×0x400` (1 KiB per core).

Stage-2 countdown (`WDT_TIMEOUT` / `WDT_TIMEOUT_RESET`) lives in
`cpu_ctrl` / `smc_cpu_cluster`, not in this IP.

Architecture, CSRs, and programming are in the hardware TRM. Model and
test docs:

- `doc/index.adoc` — entry (includes the two pages below)
- `doc/implementation.adoc` — SystemC/TLM model and `smc-vp` bind
- `doc/test_plan.adoc` — standalone cases and firmware tests

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
    ├── index.adoc
    ├── implementation.adoc
    └── test_plan.adoc
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
./run_tests.sh              # Release build + run
./run_tests.sh --ctest
./run_tests.sh --asan
./run_tests.sh --coverage   # do not combine with --asan
./run_tests.sh --clean
```

`run_tests.sh` probes `SYSTEMC_HOME` / `CCI_HOME`. Platform firmware:
`cd sw/smc-vp-tests && ./run_smc_vp_tests.sh smc-wdt-test`. Full
commands are in `doc/test_plan.adoc`.

## References

- `hw/smc/smc_cpu/data/registers/rdl/wdt.rdl`
- `OCAH*WatchdogTimer.sv` / `OCAH*TLWDT.sv`
- `smc/doc/systemc_tlm2_integration_guide.adoc`
