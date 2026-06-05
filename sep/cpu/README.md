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
cd sep/peripherals/cpu
mkdir build && cd build
cmake .. 
make
```

Produces `libveeriss_model.a`. Normally built as part of the full VP:

```bash
cd riscv-vp-plusplus/vp/build
cmake .. && make sep-vp
```

## Test

No standalone unit tests — functional validation is done at the VP level:

```bash
cd riscv-vp-plusplus/sw/sep-vp-tests/<test>
make sim
```

See `docs/specification/VeeR_ISS_Integration.md` for upstream commit info and SEP-specific patches applied to `VeeR-ISS/`.
