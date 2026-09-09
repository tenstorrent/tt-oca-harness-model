# SEP firmware tests from tt-oca-harness

The tests under `sw/sep-vp-tests/` are written by the people who build the
virtual platform. The tests in *this* directory are the firmware tests from the
`tt-oca-harness` repo (`fw/sep/tests`), written by the RTL and firmware teams, and
run here against `sep-vp`. Running both suites means the VP is checked against
software that was not written with the VP in mind.

Everything needed to build and run them is either in this directory or built by
`dependencies/setup_dependencies.sh`. There is no dependency on a `tt-oca-harness`
checkout, a VeeR EL2 checkout, `bender`, or anything installed system-wide
beyond a RISC-V toolchain, `git` and `python3`.

This directory is also where the SEP register headers now live, in
`dependencies/meta/registers/c`. They came from the `tt-oca-harness` tree that this
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
located by walking up to the `tt-oca-harness-model` root. Override with `GCC_PREFIX`,
`RISCV_TOOLCHAIN_PATH`, `SEP_VP` or `CONFIG` if you need something else.

## Layout

```
fw-tests-from-tt-oca-hw/
├── fw/sep/                     copy of tt-oca-harness fw/sep — the tests themselves
│   ├── tests/
│   │   ├── run_all_tests.sh    build + run everything, print a summary
│   │   ├── run_test.sh         build + run one test
│   │   ├── vp_test_env.sh      shared path / toolchain / dependency discovery
│   │   ├── common/             common.mk, crt0.s, init_stdout.c, …
│   │   └── <test>/             one directory per test
│   └── bootcode/               the SEP Boot ROM (BL0) — separate build, see below
├── dependencies/               everything fw/sep needs from the rest of tt-oca-harness
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

**picolibc.** Nearly every test uses `printf`, and `common/init_stdout.c`
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
  discovery, shared by both runner scripts. Also `vp_base_config`, which hands
  the three `spi_ot_flash_*` tests `accellera_config_no_spipreload.ini`: the
  default config preloads the boot image into the SPI flash so the ROM flow has
  something to boot from, and since the flash model can only clear bits (as real
  flash does), a test that programs its own pattern over preloaded bytes reads
  back `pattern & boot_byte`. The RTL testbench starts from erased flash, so
  upstream never has to think about this.

  `vp_base_config` also mirrors the one eFuse override in the testlist. The VP
  preloads the fuse array with `default_efuse.preload`, the same image
  `sep_test_template` loads via `+sep_preload_efuse`, but the three
  `sep_efuse_fw_*` tests override their `run_flags` with `+SEP_EFUSE_NO_PRELOAD`
  because they program fuses themselves and have to start from a blank part: a
  preloaded field cannot be programmed to a different value, since fuses only go
  0->1. `vp_efuse_blank_config` derives that config with `sed`, taking a copy of
  both `accellera_config.ini` and the `efuse_vp.ini` it includes, since the image
  is named in the latter.
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
- **`rom_no_tcm_preload_mem_init/Makefile`** — the same `PYTHON3`, for the same
  reason. This is one of the two tests that does not include `common.mk`, so it
  hardcoded `python3` and never saw the venv.
- **`fw/smc/tests/*/src/*_protocol.h`** (added) — six SMC/SMU co-tests under
  `fw/sep/tests` include a protocol header from the *`fw/smc`* tree, which this
  copy does not otherwise carry. Their Makefiles already point at
  `$(OCH_ROOT)/fw/smc/tests/<name>/src`, so mirroring just the headers there is
  enough to build them.

### Per-test deviations that must survive a re-sync

Everything above is infrastructure. These are edits to test *sources*, which is
a higher bar, so each one is justified individually below. All are commented in
place. Re-syncing them from `tt-oca-harness` verbatim has been tried and measured:
it turns each of these back into a failure or a hang.

| Test | Why it differs from upstream |
|---|---|
| `kmac_prefix_test` | Upstream's `PREFIX` does not start with `encode_string("KMAC")`, which NIST SP 800-185 requires. The RTL feeds raw `PREFIX` bytes into `bytepad` without checking, so upstream gets a different digest by accident; the VP model raises `IncorrectFunctionName` (ERR 0x07) and the result is undefined. The local value is the conforming encoding. |
| `kmac_key_length_test` | Upstream configures `mode = 0x2`. Per `kmac.hjson` a KMAC operation needs cSHAKE (`0x3`) with `kmac_en = 1`; with `0x2` the key length is not consumed and every key length yields the same digest. |
| `local_alias_sanity` | Upstream still assumes the pre-#3711 alias base of `0xC000_0000` with `target_base = 0`. The register header it now ships with has already moved to `0xD000_0000` / `0x1000_0000`, so the upstream test contradicts its own header. |
| `wdt_count_overflow_test` | Step 6 needs NMI and reset to arrive at distinguishable times. The VP fires both in one delta cycle because the power-manager latency that separates them on silicon is not modelled, so the upstream step waits forever. The local step 6 tests near-max counter preload plus bark and pet, with `BARK_THOLD < BITE_THOLD`. |
| `wdt_cfg_lock_test` | Uses the mailbox `nmi_set_vector()` rather than `nmi_set_vector_reg()` / `nmi_lock_vector_reg()`, and a `0x1000` bark threshold. With the upstream pair the test hangs rather than reaching its assertions. |

When a re-sync overwrites one of these, the symptom is a hang or a fresh failure
in that single test — not a build break — so it is easy to miss. Check this
table first before investigating.

## The Boot ROM

`fw/sep/bootcode/` is BL0: the code that runs out of reset, validates a manifest
in SPI flash and jumps to BL1. It is not part of `run_all_tests.sh` — it has its
own build and its own VP wiring, both described under "Building and running the
SEP Boot ROM (SPI boot)" in the top-level `README.md`. In short:

```bash
cd fw/sep/bootcode
make GCC_PREFIX=riscv64-elf BOOT_SPI_CONTROLLER_OT=1 all

cd ../../../../../..            # tt-oca-harness-model
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

The tree now mirrors all 181 test directories in `tt-oca-harness/fw/sep/tests`.
A full run is **not 100%** (typically around 90% pass) because of known VP
and harness gaps below. Exact counts move as models land. Each test has a
log in `fw/sep/tests/logs/<test>.log`. `run_all_tests.sh` still exits 0.

### Which failures are the VP's fault

Not every test in `fw/sep/tests` runs in the *standalone* SEP testbench, and the
distinction decides whether a failure here means anything. The authority is
`tt-oca-harness/dv/sep/tb/tb_uvm/yaml/testlist_sep.yaml` versus
`dv/smu/tb/tb_uvm/yaml/testlist_smu_chiplet.yaml`. Nineteen of these tests are
enrolled only in the SMU chiplet list, so even on RTL they need SMU + SMC + SEP
elaborated together; they are excluded rather than reported (see below).

Of what remains, a test that passes in the standalone SEP TB *should* pass here,
with one caveat: that testbench is not firmware-alone. It supplies eFuse and
shadow-register preloads, `+CRYPTO_EDN_HACK`, a `+ROM_IFU_PATTERN` that writes
instruction patterns into the boot ROM, cocotb handshakes that inject config
through scratch registers, a second CPU (the key manager) running its own ROM, a
UVM master driving external AXI, and an explicitly selected Winbond flash model.
So the remaining failures split into two kinds:

| Kind | Count | Tests |
|---|---|---|
| **VP model gap** — the VP should be fixed | 2 | `hmac_p2_sensreg_access_test`, `otbn_sw_error_test` |
| **VP harness gap** — the RTL TB provides something `sep-vp` has no equivalent for | 6 | `rom_sanity_test` (ROM instruction-pattern preload), `sep_aes_mb_stream_test` (cocotb scratch handshake), `sep_cpu_sram_aes_sram_test` and `sep_km_efuse_coexist_test` (key-manager CPU + its ROM), `lcc_inbound_filter_gating_test` and `sep_inbound_filter_decerr` (UVM master driving external AXI; both hang waiting for it) |

Adams Bridge (`sep_abr_*`) is modeled: `abr_ip` is bound at `0x1094_0000` (PIC 35/36)
with a FIPS 204/203 backend and key-manager DEST `0x10`/`0x20`/`0x40`/`0x80`
sideload. The six firmware tests are in the `run_all_tests.sh` discovery set.

The remaining model gaps in detail. The three eFuse tests that used to be here
now pass: the shim moved to `0x2000_0000` in `SEP_EXTERNAL` where the register
header puts it, a real fuse array sits behind program and read with the locks
and token matching enforced against it, and the array is preloaded from the
RTL's own `default_efuse.preload` — with the three `sep_efuse_fw_*` tests
running on a blank array, as their `+SEP_EFUSE_NO_PRELOAD` asks for.
`sep_aes_reset_clear_test` and `sep_reset_ctrl_csr_test` also pass on current
`sep-vp`.

- **HMAC.** `DIGEST_0..7` accept and echo software writes outside a context
  restore, where silicon ignores them.
- **OTBN.** `ERR_BITS` stays zero after a `BAD_DATA_ADDR`; software errors are
  not reported.

### Excluded, and why

`bl1_pass_test` is the BL1 boot flow rather than a VP test, and `common`,
`common_otbn`, `logs` and `otbn_km_sideload_keydump` (an `otbn_src/` asset tree
with no Makefile, absent from `tt-oca-harness/fw/sep/Makefile` too) are not tests.
Beyond those:

- **Cadence xSPI (12).** `xspi_flash_*`, `spi_sanity`, `spi_sanity_cadence`,
  `spi_write_read_test`, `spi_phy_reg_test`, `spi_crc_test`, `spi_xspi_dma_test`,
  `firmware_spi_dma_test`. The VP models the OpenTitan SPI host and its flash;
  the Cadence controller, its PHY and the XIP region are out of scope. Left
  un-synced, so their sources still use the pre-rename `SEP_AXI_EXTENSION_*`
  macros and would not compile against the current header. The 27 `spi_ot_*`
  tests and `spi_sanity_ot` are unaffected.
- **Six fabric P3 tests.** Do not compile upstream either:
  `tt-oca-harness/fw/sep/Makefile` keeps the same set out of its own `DEFAULT_TESTS`
  "temporarily disabled due to compilation errors", because each calls a helper
  nobody defined. Synced verbatim so the mirror stays honest; they return when
  upstream repairs them.
- **`uart`.** Needs `uart_16550_*_reg.h`, which `tt-oca-harness` generates on demand
  and does not commit, and the VP models no UART — `printf` goes to the
  `0x8000_0000` mailbox instead.
- **Eighteen SMU/SMC-level tests.** `sep_smu_*` (13), `sep_smc_interop`,
  `sep_smc_mbox_irq`, `sep_smc_xbar`, `smu_cla_sep_cpu_debug`,
  `smu_smc_stall_sep`. Wrong testbench, not a VP defect: these are enrolled in
  `testlist_smu_chiplet.yaml`'s `Main_SMU_Level_SEP_Regression`, not in
  `testlist_sep.yaml`. SEP firmware arrives via `+SEP_ITCM_HEX_FILE` while SMC
  runs its own `+FW_TEST`, and the cocotb harness judges pass/fail from the SEP
  program counter against a symbol table rather than the stdout banner `sep-vp`
  watches for. `sep_smc_notify` is deliberately *not* excluded: it needs no peer
  and passes here.
- **`global_alias_remap_sanity`.** Superseded upstream, and in no regression
  list: `tt-oca-harness` replaced it with the pure-UVM
  `sep_global_alias_remap_uvm_test`, which runs with `+SEP_SKIP_CPU_RUN` and no
  firmware at all, having previously needed a `Force` on `security_disable` plus
  an external AXI master. A failure here would say nothing about the VP.
- **`efuse_sanity_csr_test`.** Reads `SEP_EXTERNAL_EFUSE_SHIM_CTRL_n_7` at
  `0x2000_0024`, one of the seventeen Samsung shim registers the stale
  `och_sep_top_reg.h` still describes. Current RTL gives the SEP shim one
  register, `EFUSE_BANK_INIT_TIME` at offset 0
  (`hw/ip/efuse/dv/models/regs/efuse_shim_ctrl.rdl`), so the window is `0x4` wide
  and that address decodes nowhere — the run traps with mcause 7 rather than
  failing a check. The Samsung block moved to the SMC side as
  `SMC_EXTERNAL_MANDATORY_EFUSE_SHIM_CTRL_ln`. Re-syncing the register header
  brings this back. The counts above predate this exclusion.

## Updating from tt-oca-harness

`rsync` the test directories into `fw/sep/tests/` and re-run. Three things need
care:

1. **The register header and the test sources move together.** Refreshing
   `dependencies/meta/registers/c/och_sep_top_reg.h` on its own breaks the build
   in ~46 files: upstream renamed `SEP_AXI_EXTENSION_*` to `SEP_EXTERNAL_*`,
   moved `EFUSE_SHIM_CTRL` to `0x2000_0000` and renamed `EFUSE_WRITE_CTRL` to
   `EFUSE_PROGRAM_CTRL`. Sync both and it compiles clean, because upstream has
   already made the same move.
2. **Do not overwrite the local infrastructure**: the runner scripts,
   `vp_test_env.sh`, `common/common.mk`, `common_otbn/otbn_app.mk` and
   `generate_otbn_c.py`, and the `PYTHON3` line in
   `rom_no_tcm_preload_mem_init/Makefile`.
3. **Check the per-test deviations table above** before re-syncing those five
   files. Overwriting them costs five passing tests and the symptom is a hang,
   not a build error.

If a new test pulls in a header from elsewhere in `tt-oca-harness`, add it under
`dependencies/` and, if it lives in a directory that is not already symlinked at
the top level, add the symlink. Headers from a sibling firmware tree go in the
mirrored path instead — see `fw/smc/tests/*/src`.
