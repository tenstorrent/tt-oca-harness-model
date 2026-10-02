# cpu — VeeR EL2 RISC-V Core

SystemC/TLM model of the VeeR EL2 RISC-V core used in the SEP subsystem.
Composed of two layers:

| Component | Location | Role |
|---|---|---|
| `VeeR-ISS` | `VeeR-ISS/` | Cycle-approximate C++ ISS (upstream: chipsalliance/VeeR-ISS @ e6b4fb1) |
| `VeeR-ISSTlm` | `VeeR-ISSTlm/` | SystemC TLM-2.0 wrapper; connects ISS to the rest of the platform |

## Interface (VeeR-ISSTlm)

| Port / Socket | Direction | Description |
|---|---|---|
| `initiator_socket` | Initiator | TLM-2.0 bus master for instruction + data memory |
| IRQ line (`irq_if`) | Input | External interrupt (MEI) to the core |
| GDB RSP server | TCP | Remote GDB connection (port configurable via CCI) |

**CCI parameters** (set in `accellera_config.ini`):

| Parameter | Default | Description |
|---|---|---|
| `veeriss.config_file` | `veeriss_config.json` | VeeR hart configuration (ISA, DCCM, ICCM sizes) |
| `veeriss.gdb_port` | `0` (disabled) | TCP port for GDB RSP; `0` = disabled |
| `veeriss.verbosity` | `0` | Log verbosity (0–5) |

## Build

```bash
cd sep/cpu
./run_tests.sh
```

Produces `libveeriss_model.a` and the standalone `veeriss_tb`. Normally also
built as part of the full VP (`make sep-vp`).

## Test

Standalone TLM wrapper tests (Release / ASan / Coverage ≥ 95% on
`VeeR-ISSTlm.cpp`):

```bash
cd sep/cpu
./run_tests.sh              # Release
./run_tests.sh --asan
./run_tests.sh --coverage
./run_tests.sh --no-smepmp  # core built without Smepmp (RV_SMEPMP=0)
```

The ISS models the Smepmp PMP extension (`mseccfg`) by default, as the SEP
core is built with `RV_SMEPMP 1`. Configure with `-DVEERISS_SMEPMP=OFF` to
model a VeeR EL2 instance without it.

Platform-level firmware tests remain under `sw/sep-vp-tests/` (`rom_test`,
`sep-efuse-test`, …).

See `docs/VeeR_ISS_Integration.md` for upstream commit info and SEP-specific
patches applied to `VeeR-ISS/`.
