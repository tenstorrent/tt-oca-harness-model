# SEP firmware tests from tt-oca-hw

The tests under `sw/sep-vp-tests/` are written by the people who build the
virtual platform. The tests in *this* directory are the firmware tests from the
`tt-oca-hw` repo (`fw/sep/tests`), written by the RTL and firmware teams, and
run here against `sep-vp`. Running both suites means the VP is checked against
software that was not written with the VP in mind.

Everything needed to build and run them is either in this directory or built by
`dependencies/setup_dependencies.sh`. There is no dependency on a `tt-oca-hw`
checkout, a VeeR EL2 checkout, `bender`, or anything installed system-wide
beyond a RISC-V toolchain, `git` and `python3`.

This directory is also where the SEP register headers now live, in
`dependencies/meta/registers/c`. They came from the `tt-oca-hw` tree that this
repo no longer carries, and `sw/sep-vp-tests/Makefile.common` includes them from
here, so the two SEP suites share one authoritative copy.

## Quick start

```bash
cd fw/sep/tests
./run_all_tests.sh              # build and run everything
```

The first run takes a few extra minutes: it builds picolibc into
`dependencies/`. Later runs skip straight to the tests. A full pass takes
roughly five minutes.

A single test:

```bash
./run_test.sh hello_world              # build and run
./run_test.sh spi_sanity --run-only    # reuse the existing ELF
./run_test.sh dma_test -t 60           # raise the 25 s timeout
```

Options for `run_all_tests.sh`: `--clean` (rebuild from scratch), `--no-build`
(run existing ELFs), `-t <seconds>`, `-v`. Per-test logs land in
`fw/sep/tests/logs/`.

### Prerequisites

| Tool | macOS | Ubuntu | RHEL / TT |
|---|---|---|---|
| RISC-V toolchain | `brew install riscv64-elf-gcc` | `apt install gcc-riscv64-unknown-elf` | `module load riscv-gnu-toolchain/2025.01.20-rhel-8.10` |
| `sep-vp` | `cd vp && ./configure_vp.sh && cmake --build build --target sep-vp` | same | same |

The scripts find both by themselves: the toolchain prefix is probed from `PATH`
(`riscv64-unknown-elf-`, `riscv64-elf-`, `riscv-none-elf-`, …) and `sep-vp` is
located by walking up to the `tt-oca-sim` root. Override with `GCC_PREFIX`,
`RISCV_TOOLCHAIN_PATH`, `SEP_VP` or `CONFIG` if you need something else.

## Layout

```
fw-tests-from-tt-oca-hw/
├── fw/sep/                     copy of tt-oca-hw fw/sep — the tests themselves
│   ├── tests/
│   │   ├── run_all_tests.sh    build + run everything, print a summary
│   │   ├── run_test.sh         build + run one test
│   │   ├── vp_test_env.sh      shared path / toolchain / dependency discovery
│   │   ├── common/             common.mk, crt0.s, init_stdout.c, …
│   │   └── <test>/             one directory per test
│   └── bootcode/               the SEP Boot ROM (BL0) — separate build, see below
├── dependencies/               everything fw/sep needs from the rest of tt-oca-hw
│   ├── setup_dependencies.sh   builds picolibc
│   ├── meta/registers/c        SEP register headers (also used by sw/sep-vp-tests)
│   ├── dv/sep/tests/           common + common_otbn test infrastructure
│   ├── vendor/opentitan/       OTBN assembler and data files
│   ├── el2/                    stands in for the VeeR EL2 checkout (see below)
│   └── build/                  scratch: picolibc source, build tree, venv
└── deps, dv, meta, vendor      symlinks into dependencies/
```

The symlinks at the top exist so the test Makefiles keep working unchanged:
they refer to `$(OCH_ROOT)/meta/...`, `$(OCH_ROOT)/dv/...` and so on, and
`OCH_ROOT` points at this directory.

## What `setup_dependencies.sh` builds, and why it has to

Almost everything the tests need could be copied. One thing could not.

**picolibc.** Nearly all 124 tests use `printf`, and `common/init_stdout.c`
wires `stdout` to the VP's STDOUT register with picolibc's `FDEV_SETUP_STREAM`,
so the library is not interchangeable with newlib. No RISC-V toolchain ships
it — upstream builds it inside the VeeR EL2 checkout via `bender` and meson.
The script builds it straight from the picolibc release instead (pinned to
1.8.10) and installs it in the layout `common.mk` expects, under
`dependencies/el2/third_party/picolibc/install`. meson, ninja and the python
packages the OTBN tools need go into a throwaway venv in `dependencies/build`,
so nothing is installed on the machine.

The script also writes two stand-ins for the VeeR EL2 checkout that `common.mk`
insists on: `el2/snapshots/sep/defines.h` (a placeholder — the only consumer is
`crt0_bl1.s`, part of the BL1 flow the VP does not run) and `el2/tools/picolibc.mk`
(upstream builds picolibc; here it just checks that picolibc is present).

Both the venv and `picolibc.specs` record absolute paths, so renaming or moving
this directory breaks them — the venv with `bad interpreter`, the specs with
`cannot find -lc` at link time. The runners notice a specs file that points
somewhere else and re-run the setup, which rewrites the paths and rebuilds the
venv without rebuilding picolibc itself. Re-run with `--force` to rebuild
everything from scratch.

## Changes made to the upstream sources

Kept to a minimum, and all of them are about running somewhere other than a TT
machine. Every one is commented in place.

- **`fw/sep/tests/vp_test_env.sh`** (new) — path, toolchain and `sep-vp`
  discovery, shared by both runner scripts.
- **`run_test.sh` / `run_all_tests.sh`** — resolve paths through
  `vp_test_env.sh` rather than counting `../`; replaced bash 4 associative
  arrays and GNU `timeout(1)`, neither of which macOS has, with a polling loop
  that also stops the VP once the firmware prints its banner. Without that the
  scripts hang forever, because `sc_start()` keeps the SystemC kernel running
  after the firmware finishes.
- **`common/common.mk`** — probe whether the assembler accepts CSR instructions
  with the effective `-march` and re-add `_zicsr_zifencei` when it does not.
  Toolchains from 2022 on split Zicsr out of the base ISA, so the several tests
  that force plain `-march=rv32imc` no longer assemble on them.
- **`common_otbn/otbn_app.mk`** — use the installed binutils rather than
  assuming the `riscv32-unknown-elf` spelling, pass `-m elf32lriscv` so a
  riscv64 `ld` accepts the rv32 objects, and run the OTBN python tools with the
  venv interpreter.
- **`dv/sep/tests/common_otbn/generate_otbn_c.py`** — take `objcopy` from
  `RV32_TOOL_OBJCOPY`, the same variable the OpenTitan tools already use.
- **`fw/sep/bootcode/Makefile`** — the same Zicsr probe as `common.mk` (`vector.S`
  is full of CSR writes and the ROM builds with a plain `-march=rv32im`), and a
  `PYTHON3` that prefers the venv, since `tools/elf-to-vmem.py` now needs
  pyelftools.

## The Boot ROM

`fw/sep/bootcode/` is BL0: the code that runs out of reset, validates a manifest
in SPI flash and jumps to BL1. It is not part of `run_all_tests.sh` — it has its
own build and its own VP wiring, both described under "Building and running the
SEP Boot ROM (SPI boot)" in the top-level `README.md`. In short:

```bash
cd fw/sep/bootcode
make GCC_PREFIX=riscv64-elf BOOT_SPI_CONTROLLER_OT=1 all

cd ../../../../../..            # tt-oca-sim
vp/build/bin/sep-vp vp/platform/sep/config/accellera_config.ini \
    sw/sep-vp-tests/fw-tests-from-tt-oca-hw/fw/sep/bootcode/build/boot_rom.elf
```

`BOOT_SPI_CONTROLLER_OT=1` is required: it selects the OpenTitan SPI host driver
(`sep_ot_spi.c`), and the VP models only the OpenTitan SPI peripherals, so a ROM
built with the default Cadence driver never gets past SPI init. Drop
`GCC_PREFIX` wherever `riscv64-unknown-elf-` is the installed spelling.

A good run walks `BOOT_SPI` → `SPI_INIT_OK` → `MANIFEST_OK` → `BL1_FOUND` →
`BL1_COPIED` → `BL1_JUMP=0x10020000` → BL1's `GO!` → `SIMULATION OF THE TEST
PASSED`. The ROM then parks the core in `wfi`, so the run needs a `Ctrl-C`.

## Current results

All 124 test directories build, and 125 binaries run — `spi_crc_test` and
`spi_phy_reg_test` each produce a second OT-mode ELF. 101 pass. `bl1_pass_test`
is excluded upstream (it is the BL1 boot flow, not a VP test), as are the
`common`, `common_otbn` and `logs` directories.

Upstream's `uart` test is not carried over: SEP has no UART model, so there is
nothing for it to exercise. Its register headers were the only reason this
directory needed the SystemRDL generator, which is gone with it.

The 24 failures are functional differences between the VP models and the RTL
these tests were written against, reported by the firmware itself — not build or
environment problems. They cluster by IP:

| Area | Failing tests |
|---|---|
| SPI / XSPI flash | `spi_sanity`, `spi_sanity_cadence`, `spi_crc_test`, `spi_phy_reg_test`, `spi_write_read_test`, `spi_xspi_dma_test`, `firmware_spi_dma_test`, `xspi_flash_jedec_id_test`, `xspi_flash_read_test`, `xspi_flash_sram_loopback_test`, `spi_ot_flash_dual_read_test`, `spi_ot_flash_quad_read_test`, `spi_ot_flash_write_read_test` |
| DMA | `dma_hash_test` |
| eFuse | `sep_efuse_fw_otp_rw_test`, `sep_efuse_fw_token_match_test` |
| WDT | `wdt_cfg_lock_test`, `wdt_intr_clear_test` |
| Address remap | `ap_stee_output_remap_test`, `global_alias_remap_sanity` |
| Other | `rom_sanity_test`, `hmac_p2_sensreg_access_test`, `kmac_p2_sw_error_test`, `otbn_sw_error_test` |

Each has a log in `fw/sep/tests/logs/<test>.log`.

## Updating from tt-oca-hw

Copy the new or changed test directories into `fw/sep/tests/` and re-run.
Re-apply the `common.mk` / `otbn_app.mk` changes above if those files are part
of the update; the runner scripts and `vp_test_env.sh` are local and should not
be overwritten. If a new test pulls in a header from somewhere else in
`tt-oca-hw`, add it under `dependencies/` and, if it lives in a directory that
is not already symlinked at the top level, add the symlink.
