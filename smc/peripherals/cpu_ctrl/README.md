# SMC CPU Control — SystemC / TLM-2.0 Loosely-Timed Model

SystemC/TLM-2.0 LT model of the SMC **CPU Control** register block, including
the **`SCRATCH[16]`** array used for **SMC ROM ↔ SEP ROM inter-stage handoff**.

## Authoritative sources

- `tt-oca-hw/hw/smc/smc_misc/data/registers/rdl/cpu_ctrl.rdl` — register map
- `tt-oca-hw/fw/smc/prod_rom/specification/smc_rom.adoc` — scratch handoff protocol
- `tt-oca-hw/fw/smc/prod_rom/include/smc_rom_defs.h` — index / bit definitions

## Layout

```
cpu_ctrl/
├── CMakeLists.txt
├── README.md
├── run_tests.sh
├── include/
│   └── cpu_ctrl.h
├── src/
│   └── cpu_ctrl.cpp
├── test/
│   └── cpu_ctrl_tb.cpp
└── doc/
    ├── 01_CPU_CTRL_Specification.md
    ├── 02_CPU_CTRL_LowLevel_Design.md
    └── 03_CPU_CTRL_Test_Plan.md
```

## Quick start

```bash
export SYSTEMC_HOME=/path/to/systemc-cxx20
export CCI_HOME=/path/to/cci
./run_tests.sh
```

## Fabric binding

```cpp
smc::cpu_ctrl cpu("cpu_ctrl");
fabric.to_cpu_ctrl.bind(cpu.reg_socket);
```

Preset `base_addr` to `0xC0010000` (default) so absolute addresses from the
fabric decode correctly.

## Handoff scratch (index → role)

| Index | Role |
|-------|------|
| 8 | Manifest SRAM offset |
| 9 | SMC → SEP status bits |
| 11 | Status buffer offset |
| 13 | SEP safe SRAM start offset |
| 14 | SEP safe SRAM size |
| 15 | Memory repair status |

See `doc/01_CPU_CTRL_Specification.md` for full register map and bus contract.
