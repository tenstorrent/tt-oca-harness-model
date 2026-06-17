# SMC Fabric — SystemC / TLM-2.0 Loosely-Timed Model

A standards-compliant **SMC Fabric** model in Accellera SystemC 2.3.x / 3.0 +
TLM-2.0 (Loosely-Timed). It is the central interconnect for the SMC SystemC IP
library: dual-aperture address routing (local + global alias), alias remap,
M-mode / Xvisor output remap, inbound/outbound AXI filters, and local crossbar
decode to front-port peripherals, CSRs, and the system NoC.

The model is specified and validated against the authoritative RTL under
`sw/tt-oca-hw-main/`:

- `hw/smc/smc_fabric/smc_fabric.sv` — top fabric wrapper.
- `hw/smc/smc_fabric/smc_input_fabric/rtl/smc_input_fabric.sv` — internal-master
  alias remap and the fixed **16 MB** local/outbound demux
  (`LOCAL_ALIAS_REGION_SIZE` in `hw/smc/smc_pkg.sv`).
- `hw/smc/smc_fabric/smc_output_fabric/rtl/smc_output_fabric.sv` — M-mode / Xvisor
  output remap demux and outbound filter path.
- `hw/ip/output_remap/rtl/output_remap.sv` and
  `hw/ip/output_remap/data/registers/rdl/output_remap.rdl` — bit-exact
  `REGION_ATTRS.offset[55:0]` encoding.
- `hw/comp/axi_filter/data/registers/rdl/filter_ctrl.rdl` — bit-exact
  `FILTER_CONFIG` / `START_ADDR` / `END_ADDR` layout.
- `hw/smc/smc_config_pkg.sv` — `NO_ADDR_REMAP = 0` (output remap enabled).
- `meta/crossbars/smc_local_xbar.sv` and
  `meta/crossbars/configs/smc_local_xbar.yaml` — local address decode map.
- `dv/smc/tb/tb_uvm/uvm_tests/smc_output_fabric_wr_rd_test.sv` and
  `dv/common/smc_uvm/smc_scoreboard/smc_overall_scoreboard.sv` — RTL verification
  reference for outbound remap behaviour.

The module is a drop-in `SC_MODULE` that the rest of the SMC SystemC tree wires
up like the PLIC, CLINT, and Boot ROM blocks. Downstream initiator sockets bind
to peripheral models; remap/filter CSR writes update internal tables directly.

---

## Layout

```
smc_fabric/
├── CMakeLists.txt
├── README.md                       (this file)
├── run_tests.sh                    Build + run helper (Release / ASan / coverage)
├── include/
│   └── smc_fabric.h                SC_MODULE(smc_fabric) + config struct
├── src/
│   └── smc_fabric.cpp              Routing, remap, filter, CSR handlers
├── test/
│   ├── CMakeLists.txt
│   └── smc_fabric_tb.cpp           Self-checking test bench (13 scenarios)
└── doc/
    ├── README.md                   Document index
    ├── build_docs.sh               Markdown → PDF (pandoc + Chrome)
    ├── print.css                   PDF stylesheet
    ├── 01_overview_and_architecture.md
    ├── 01_overview_and_architecture.pdf
    ├── 02_tlm_interface.md
    ├── 02_tlm_interface.pdf
    ├── 03_internal_architecture.md
    ├── 03_internal_architecture.pdf
    ├── 04_register_interface.md
    ├── 04_register_interface.pdf
    ├── 05_systemc_implementation.md
    └── 05_systemc_implementation.pdf
```

---

## Module interface

```cpp
SC_MODULE(smc_fabric) {
    // Inbound (6 target sockets)
    tlm_utils::simple_target_socket<smc_fabric, 64> jtag_axi_in;
    tlm_utils::simple_target_socket<smc_fabric, 64> mmio_in;
    tlm_utils::simple_target_socket<smc_fabric, 64> data_accel_in;
    tlm_utils::simple_target_socket<smc_fabric, 64> log_in;
    tlm_utils::simple_target_socket<smc_fabric, 64> sys_axi_in;
    tlm_utils::simple_target_socket<smc_fabric, 64> sep_axi_in;

    // Outbound initiators (local targets + output_axi)
    tlm_utils::simple_initiator_socket<smc_fabric, 64> to_front_port;
    tlm_utils::simple_initiator_socket<smc_fabric, 64> to_periph;
    tlm_utils::simple_initiator_socket<smc_fabric, 64> output_axi;
    // … plus data_accel, dfd, cpu_ctrl, mailbox, dft, remap/filter CSR stubs

    sc_in<bool> rst_n_i;   // active-low; clears programmed tables

    explicit smc_fabric(sc_module_name, config = config{});
};
```

`config` defaults match the shipped SMC RTL parameters:

| Field | Default | Note |
|-------|---------|------|
| `local_base_addr` | `0xC000_0000` | Local alias base (`LOCAL_BASE` CSR, RO) |
| `global_base_addr` | `0x4000_0000` | Global alias base (`GLOBAL_BASE` CSR) |
| `region_size` | `0x0200_0000` | `REGION_SIZE` CSR image only — **not** used for routing |
| `no_addr_remap` | `false` | Matches `smc_config_pkg::NO_ADDR_REMAP` (remap on) |
| `reg_access_ns` | `1.0` | TLM delay added per CSR access |

Routing uses a fixed **16 MB** local alias window
(`LOCAL_ALIAS_REGION_SIZE = 0x0100_0000`), matching `smc_pkg.sv`, independent of
the `region_size` CSR.

Instantiate with custom parameters when needed:

```cpp
smc::smc_fabric::config cfg;
cfg.global_base_addr = 0x4000'0000ULL;
cfg.no_addr_remap    = true;   // bypass M-mode / Xvisor tables (test-only)
smc::smc_fabric fabric("fabric", cfg);
```

See `doc/02_tlm_interface.md` for the full socket catalogue and
`doc/04_register_interface.md` for the CSR map.

---

## Behaviour highlights

- **Internal masters** (`jtag_axi_in`, `mmio_in`, `data_accel_in`, `log_in`):
  alias remap → fixed 16 MB local/outbound split → local decode or outbound path.
- **`sys_axi_in`**: inbound filter (default deny) → local decode only.
- **`sep_axi_in`**: bypasses alias remap and inbound filter → local decode only.
- **Alias remap**: 8 programmable windows; additive offset + valid bit.
- **Output remap**: 8 M-mode + 8 Xvisor regions; bit-exact
  `{offset[55:20], addr[19:0]}` from `output_remap.rdl`.
- **Filters**: 16 inbound + 16 outbound entries; bit-exact `filter_ctrl.rdl`
  image (`FILTER_CONFIG`, `START_ADDR`, `END_ADDR`, `locked` woset).
- **DMI**: always denied on all target sockets.
- **Reset**: clears alias, output-remap, and filter tables; restores CSR defaults.

Shared AXI metadata travels in the optional `smc_axi_extension` GP extension
(from `smc/cpu_cluster/include/smc_axi_extension.h`).

---

## Build & test

```bash
./run_tests.sh                   # Release build + run smc_fabric_tb
./run_tests.sh --ctest           # Run via ctest
./run_tests.sh --asan            # AddressSanitizer (+ LSan on Linux)
./run_tests.sh --coverage        # LLVM source-based coverage (Clang) or gcov
./run_tests.sh --clean           # Wipe build/ first
```

`SYSTEMC_HOME` is auto-probed for common macOS (Homebrew) and Linux
(`/usr/local`, `/usr`) install locations; set it manually for a non-standard
prefix. The linked SystemC library must be built with **C++20**.

Manual equivalent:

```bash
export SYSTEMC_HOME=/path/to/systemc-cxx20   # if not on default search path
cmake -S . -B build -DSMC_CXX_STANDARD=20
cmake --build build -j
./build/test/smc_fabric_tb
```

For IDE navigation, point `compile_commands.json` at `build/` (generated by
CMake). A workspace `.clangd` file may already reference this path.

Regenerate PDF copies of the design documents:

```bash
./doc/build_docs.sh
```

Requires **pandoc** and **Google Chrome** or **Chromium** (headless print).
On Linux, `google-chrome-stable` is auto-detected; set `CHROME=/path/to/chrome`
to override. If pandoc is not on `PATH`, the script also probes
`.tools/pandoc-*/bin/pandoc` under this directory.

---

## Test bench

`test/smc_fabric_tb.cpp` runs 13 self-checking scenarios:

| # | Coverage |
|---|----------|
| 1 | Reset / power-on defaults |
| 2 | Local decode routing (periph, SPM, DMA, DFD via global alias, DFT, mailbox) |
| 3 | `GLOBAL_BASE` CSR vs `cpu_ctrl` forwarding |
| 4 | Alias remap hit + debug |
| 5 | Outbound default-allow (plain + M-mode identity remap at reset) |
| 6 | Inbound filter allow/deny + NS filtering |
| 7 | `sep_axi_in` bypasses inbound filter |
| 8 | Outbound filter deny |
| 9 | Unmapped local address → `TLM_ADDRESS_ERROR` |
| 10 | DMI denied |
| 11 | Reset clears programmed tables |
| 12 | M-mode / Xvisor output remap (bit-exact `offset[55:0]`) |
| 13 | Filter CSR image read-back + `locked` freeze |

Prints `ALL TESTS PASSED` on success; exits non-zero on any `EXPECT_*` failure.

---

## Current status

| Metric | Value |
|--------|-------|
| Test scenarios | 13/13 PASS |
| Output remap | Bit-exact to `output_remap.sv` / `.rdl` |
| Filter CSRs | Bit-exact to `filter_ctrl.rdl` |
| Local/outbound split | Fixed 16 MB (`LOCAL_ALIAS_REGION_SIZE`) |
| Default remap | Enabled (`no_addr_remap = false`) |

---

## Documentation

Detailed design notes live under `doc/` — each topic has a **Markdown source**
and a matching **PDF** (regenerate with `./doc/build_docs.sh`):

| Markdown | PDF | Contents |
|----------|-----|----------|
| [doc/01_overview_and_architecture.md](doc/01_overview_and_architecture.md) | [01_overview_and_architecture.pdf](doc/01_overview_and_architecture.pdf) | Block diagram, data-path flows |
| [doc/02_tlm_interface.md](doc/02_tlm_interface.md) | [02_tlm_interface.pdf](doc/02_tlm_interface.pdf) | Sockets, config, extension usage |
| [doc/03_internal_architecture.md](doc/03_internal_architecture.md) | [03_internal_architecture.pdf](doc/03_internal_architecture.pdf) | Sub-block decomposition |
| [doc/04_register_interface.md](doc/04_register_interface.md) | [04_register_interface.pdf](doc/04_register_interface.pdf) | Full CSR / filter / remap map |
| [doc/05_systemc_implementation.md](doc/05_systemc_implementation.md) | [05_systemc_implementation.pdf](doc/05_systemc_implementation.pdf) | Integration guide |

See also [doc/README.md](doc/README.md) for the document index.

---

## Citation

Modelled after the SMC PLIC, CLINT, and Boot ROM SystemC sub-packages in
`smc/peripherals/`. See those READMEs for shared SMC IP conventions (CMake
layout, SystemC C++20 ABI, error taxonomy, build options).
