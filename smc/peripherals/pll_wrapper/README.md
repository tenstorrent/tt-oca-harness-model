# SMC `pll_wrapper`

SystemC/TLM-2.0 register models for the SMC PLL wrapper, transcribed from the
RDL under `sw/tt-oca-hw-main/hw/smc/dv_shims/pll/data/registers/rdl/`
(`pll_wrap.rdl` and its includes `pll_cntl.rdl`, `cgm.rdl`, `awm.rdl`).

These are **pure register-file models**: they store register state and enforce
the software RO/RW/WO contract (plus self-clearing / single-pulse strobes) via
the shared `regmodel` helpers (`common/include/reg_access.h`, `reg_map.h`).
There is no PLL/clock datapath — hardware-driven status bits (lock detect,
monitor counters, ...) stay at their reset value and can be driven from a test
or an enclosing model through the `poke()` back door.

## Models

| Class                | RDL          | Bus width | Window  | Notes                                   |
|----------------------|--------------|-----------|---------|-----------------------------------------|
| `smc::pll::pll_cntl` | `pll_cntl.rdl` | 32-bit  | `0x88`  | CGM/AWM status, mux/clock counters      |
| `smc::pll::cgm`      | `cgm.rdl`    | 16-bit    | `0x94`  | Clock generation module CSRs            |
| `smc::pll::awm`      | `awm.rdl`    | 16-bit    | `0x500` | GLOBAL + 6× FREQUENCY + 3× CGM sub-map   |
| `smc::pll::pll_wrapper` | `pll_wrap.rdl` | 32-bit | `0x1000` | Composes the three below                 |

All four share `smc::pll::reg_block` (`include/pll_reg_block.h`), a table-driven
32-bit register-file `SC_MODULE` (the SMC register bus is 32-bit APB-style, so
16-bit RDL registers occupy the low half of a 32-bit word).

### `pll_wrapper` composed map (from `pll_wrap.rdl`)

| Offset  | Sub-block  |
|---------|------------|
| `0x000` | `pll_cntl` |
| `0x100` | `cgm_0`    |
| `0x200` | `cgm_1`    |
| `0x400` | `awm_0`    |
| `0xA00` | `awm_1`    |

The wrapper presents one 32-bit target, decodes each access to the owning
sub-block window, rebases to the block-local offset, and forwards over an
internal initiator socket (same pattern as the SMC platform fabric).

## Ports

- `reg_socket` — 32-bit TLM target (naturally aligned 4-byte accesses only).
- `rst_n_i`    — active-low reset (restores every register to its RDL default).

## Build & test

```sh
cp deps.env.example deps.env     # point SYSTEMC_HOME / CCI_HOME at your installs
./run_tests.sh                   # incremental build + run the test bench
./run_tests.sh --clean           # fresh configure/build/run
./run_tests.sh --asan            # AddressSanitizer build
./run_tests.sh --coverage        # line-coverage report
```

When built standalone the CMake project builds `libsmc_pll_wrapper.a` plus the
`pll_wrapper_tb` test bench (covering the composed map). When added as a
subdirectory (e.g. under `vp/platform/smc`) only the library target is built.

## Dependencies

- SystemC 3.0.2, CCI 1.0.1 (matches the rest of the SMC IP suite).
- C++17/20; CMake 3.16+.
