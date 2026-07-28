# SMC Bus Error Unit (BEU) — SystemC / TLM-2.0 Loosely-Timed Model

A per-core **Bus Error Unit** modeled in Accellera SystemC 2.3.x / 3.0.x +
TLM-2.0 (Loosely-Timed). It implements one instance of the SMC CPU-cluster BEU
(the Rocket-chip `BusErrorUnit` behind an AXI4-Lite register window; the SMC
packs one per core at `0xC801_0000 + N*0x1000`) as described in:

- `doc/01_BEU_Specification.md` — externally-observable behaviour
- `doc/02_BEU_LowLevel_Design.md` — TLM interface, register map, internals
- `doc/03_BEU_Test_Plan.md` — unit verification strategy and test list
- `doc/04_BEU_Platform_Integration_Test_Plan.md` — what is required to wire BEU into `smc-vp` and add an `smc-beu-test` platform firmware test
- `hw/smc/smc_cpu/data/registers/rdl/bus_error_unit.rdl` — authoritative register map
- `hw/smc/smc_cpu/chipyard_generated_files/4core/OCAH4CORECluster_BusErrorUnit.sv` — behaviour reference
- `hw/smc/doc/interrupts.adoc` — BEU local (NMI-like) + PLIC delivery
- `hw/smc/doc/memmap.adoc` — `0xC801_0000 + N*0x1000`, 4 KiB window each

The model is a drop-in `SC_MODULE` that the rest of the SMC SystemC IP library
wires up as specified in the low-level design. It is a **functional** model:
the register map, first-error latching into `CAUSE`/`PHYS_ADDR`, the per-source
accrued status, and the local/PLIC interrupt aggregation are reproduced
faithfully (and are bit-compatible with the RDL register layout), while the
TileLink bus interface, cache ECC syndromes, and cycle-level timing are
abstracted away.

---

## What a BEU does

The BEU is a passive **error recorder + interrupt generator** sitting on the
per-core bus. Five hardware error sources feed it:

| Source (bit) | `CAUSE` value | Meaning |
|--------------|---------------|---------|
| ICache TileLink bus (1)       | 1 (`itl_error`) | ICache bus error |
| ICache correctable (2)        | 2 (`iec_error`) | ICache ECC correctable |
| DCache TileLink bus (5)       | 5 (`dtl_error`) | DCache bus error |
| DCache correctable (6)        | 6 (`dec_error`) | DCache ECC correctable |
| DCache uncorrectable (7)      | 7 (`deu_error`) | DCache ECC uncorrectable |

On an error it (1) sets that source's sticky **accrued** status bit, and (2) if
recording is enabled for the source and `CAUSE` is currently clear, latches the
**first** error's cause and physical address. Two interrupt lines are raised
independently, each gated by its own per-source mask:

- **local** (NMI-like, bypasses the PLIC) = OR of `ACCRUED & LOCAL_ENABLE`
- **PLIC** (global)                      = OR of `ACCRUED & PLIC_ENABLE`

Software clears an interrupt by writing 0 to the relevant `ACCRUED_ENABLE`
bits, and re-arms cause/address recording by writing `CAUSE = 0`.

---

## Layout

```
beu/
├── CMakeLists.txt
├── README.md                     (this file)
├── run_tests.sh                  Build + run convenience script
├── deps.env.example              Template for local dependency paths
├── doc/
│   ├── 01_BEU_Specification.md / .pdf
│   ├── 02_BEU_LowLevel_Design.md / .pdf
│   ├── 03_BEU_Test_Plan.md / .pdf
│   ├── 04_BEU_Platform_Integration_Test_Plan.md / .pdf
│   ├── 02_BEU_LowLevel_Design.md
│   └── 03_BEU_Test_Plan.md
├── include/
│   └── beu.h                     SC_MODULE(beu) declaration
│                                 (uses shared smc/common/include/smc_axi_extension.h)
├── src/
│   └── beu.cpp                   Implementation
└── test/
    ├── CMakeLists.txt
    ├── beu_tb.cpp                Primary self-checking test bench
    └── beu_neg_tb.cpp            Negative-path / edge-case test bench
```

---

## Building and testing

First-time setup (point the build at your SystemC + CCI installs):

```bash
cp deps.env.example deps.env
# edit SYSTEMC_HOME and CCI_HOME in deps.env
./run_tests.sh
```

`run_tests.sh` options:

| Flag | Effect |
|------|--------|
| (none) | Incremental Release build + run |
| `--clean` | Wipe `build/` first |
| `--ctest` | Run via `ctest` |
| `--asan` | Build with AddressSanitizer (Linux: + LeakSanitizer) |
| `--coverage` | Build with coverage; print a line report |

Both test benches print `ALL TESTS PASSED` on success. Line coverage of
`src/beu.cpp` is 100 %.

---

## Register map (per core, 4 KiB window; registers in low 0x30)

All registers are 64-bit and accessed with 8-byte, 8-byte-aligned transactions.

| Offset | Name | SW | Notes |
|--------|------|----|-------|
| `0x00` | `CAUSE`          | rw | `[2:0]` cause of latched error; HW records the first enabled error while `CAUSE==0`; SW writes 0 to re-arm |
| `0x08` | `PHYS_ADDR`      | ro | `[55:0]` physical address of the latched error (HW-written) |
| `0x10` | `ENABLE`         | rw | `[7:0]` per-source **recording** enable (reset: all sources on = `0xE6`) |
| `0x18` | `PLIC_ENABLE`    | rw | `[7:0]` per-source PLIC (global) interrupt mask |
| `0x20` | `ACCRUED_ENABLE` | rw | `[7:0]` per-source sticky accrued error status (HW-set, SW W-clears) |
| `0x28` | `LOCAL_ENABLE`   | rw | `[7:0]` per-source local (NMI-like) interrupt mask |

Only bits `{1,2,5,6,7}` are defined in each 8-bit field; reserved bits read 0.
8-byte-aligned offsets in `[0x30, 0x1000)` decode to no register and return
`TLM_ADDRESS_ERROR_RESPONSE`.

---

## Configuration (CCI)

| CCI parameter | Type | Default | Mutability | Notes |
|---------------|------|---------|------------|-------|
| `access_delay_ns` | `cci_param<double>` | 2.0 | mutable | Annotated TLM access latency (AXI4-Lite) |

Set presets via the broker before construction:

```cpp
broker.set_preset_cci_value("top.beu0.access_delay_ns", cci::cci_value(5.0));
```

At construction the model logs an `SC_REPORT_INFO` showing the resolved value
and whether it came from a preset (`[preset]`) or the default (`[default]`).

---

## Ports

| Port | Dir | Description |
|------|-----|-------------|
| `reg_socket` | target | TLM-2.0 LT register socket (AXI4-Lite-style, 64-bit) |
| `rst_n_i` | in | Active-low asynchronous reset |
| `irq_local_o` | out | Local (NMI-like) interrupt; OR of `ACCRUED & LOCAL_ENABLE` |
| `irq_plic_o` | out | PLIC (global) interrupt; OR of `ACCRUED & PLIC_ENABLE` |

`recompute_method` is the sole driver of both IRQ outputs (single-driver
discipline).

---

## Test-bench back door

Because there is no cache in the model, the five error sources are driven
through a back door instead of ports (mirroring how `i2c_controller` abstracts
its bus behind a callback):

- `inject_error(src, phys_addr)` — emulate a hardware error event feeding this
  BEU (see `smc::beu_src`)
- `dbg_reg(off)` — side-effect-free register peek
- `dump_state(os)` — human-readable state dump

See `doc/02_BEU_LowLevel_Design.md` for details.
