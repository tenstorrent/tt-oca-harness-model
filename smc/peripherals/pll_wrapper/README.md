# SMC `pll_wrapper`

SystemC/TLM-2.0 register models for the SMC PLL wrapper, transcribed from the
RDL under `sw/tt-oca-hw-main/hw/smc/dv_shims/pll/data/registers/rdl/`
(`pll_wrap.rdl` and its includes `pll_cntl.rdl`, `cgm.rdl`, `awm.rdl`).

The three sub-blocks (`pll_cntl`, `cgm`, `awm`) are **pure register-file
models**: they store register state and enforce the software RO/RW/WO contract
(plus self-clearing / single-pulse strobes) via the shared `regmodel` helpers
(`common/include/reg_access.h`, `reg_map.h`). There is no PLL/clock datapath —
raw monitor/counter status stays at reset and can be driven from a test through
the `poke()` back door.

The composing `pll_wrapper` adds the **minimal lock behaviour the SMC firmware
depends on** (see below), so that firmware programming sequences which busy-poll
for lock make forward progress under simulation.

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

### Firmware-driven lock behaviour

The SMC firmware (`sw/tt-oca-hw-main/fw/smc`) programs a PLL and then busy-polls
`pll_cntl` status until lock, e.g. `program_cgm()` / `program_awm0_functional()`
in `common/smc_defines.h` and the `pll_*_sanity_sequence` headers. Firmware
polls the **`pll_cntl`** aggregated status — never the sub-block's own status —
so the wrapper coordinates lock across sub-blocks via a `REG_UPDATE` write hook
(`reg_block::observe_write`):

| Trigger (software write)                    | Effect (model asserts)                                  | Firmware poll exit |
|---------------------------------------------|---------------------------------------------------------|--------------------|
| `cgm_x.REG_UPDATE` (`0x20`) w/ `cgm_enable` | `pll_cntl.CGM_x_STATUS.lock_detect[0] = 1` (+ mirror `cgm.CGM_STATUS`) | `lock_detect == 1` |
| `cgm_x.REG_UPDATE` w/ `cgm_enable = 0`      | `pll_cntl.CGM_x_STATUS.lock_detect[0] = 0`              | (disable path)     |
| `awm_0.GLOBAL REG_UPDATE` (`0x28`)          | `pll_cntl.AWM_0_STATUS.lock_detect[2:0] = 7` (+ mirror `awm.GLOBAL_LOCK_STATUS`) | `lock_detect == 7` |
| `awm_1.GLOBAL REG_UPDATE`                   | `pll_cntl.AWM_1_STATUS.lock_detect[2:0] = 1`            | `lock_detect == 1` |

Lock asserts immediately on the `REG_UPDATE` strobe (loosely-timed model), so
the very next poll read terminates the loop. Reset clears all lock status.

`REG_UPDATE` itself self-clears (write-only strobe), matching the firmware
comments.

## Ports

- `reg_socket` — TLM register target. Accepts naturally aligned **1/2/4-byte**
  accesses within a single 32-bit register; the firmware uses 16-bit MMIO for
  cgm/awm and pll_cntl status, and 32-bit stores for the wide pll_cntl
  registers. Sub-word writes perform a read-modify-write on the containing word.
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
