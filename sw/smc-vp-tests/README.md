# SMC VP Software Tests

Bare-metal RV64 firmware tests for the SMC Virtual Platform (`smc-vp`).
Mirrors the shape of `sw/sep-vp-tests/` (which targets `sep-vp`), adapted for
the SMC CVA6 CPU cluster.

This directory is the **system-test** entry point for the SMC platform: the
firmware here runs on the full `smc-vp` (fabric + peripherals + CPU cluster).
The standalone per-IP unit tests live under `smc/peripherals/` and are
documented in [Running standalone SMC peripheral tests](#running-standalone-smc-peripheral-tests) below.

---

## Dependencies

The SMC VP and its tests are **not** a self-contained build — they link
against several host-installed tools. Install/point at each of the following
(the paths below are the canonical locations on this machine; set the env vars
to match your own install):

| Dependency | Env var | Version / notes |
|-----------|---------|-----------------|
| **RISC-V GNU toolchain** | `RISCV_PREFIX` | GCC 11+, RV64. Default prefix `riscv64-unknown-elf-`; on Homebrew use `riscv64-elf-`. Provides `gcc`, `objcopy`, `objdump`, `readelf`. |
| **Accellera SystemC** | `SYSTEMC_HOME` | 3.0.2 built with **C++20** (`/path/to/systemc-3.0.2-cxx20`). The ABI is keyed to the C++ standard — consumers must also be C++20. |
| **Accellera SystemC CCI** | `CCI_HOME` | CCI 1.0, C++20 (`/path/to/cci-cxx20`). Required for `cci_param` configuration. |
| **Tenstorrent Whisper ISS** | `WHISPER_HOME` | The CVA6 instruction-set simulator. Must be built with `MEM_CALLBACKS=1 CXX_STD=c++20 BOOST_ROOT="$BOOST_ROOT"` (Boost ≥ 1.74) and contain `build-<OS>/librvcore.a`. |
| **Boost** | `BOOST_DIR` | ≥ 1.74, with `iostreams` and `program_options` (`/opt/homebrew/opt/boost` on macOS). Used by Whisper headers and `smc-vp`. |
| **CMake** | — | ≥ 3.20. |
| **Python 3** | — | Used by firmware/preload generators. |

Build Whisper once (from the cpu_cluster docs):

```bash
cd "$WHISPER_HOME"
make MEM_CALLBACKS=1 CXX_STD=c++20 BOOST_ROOT="$BOOST_ROOT"
```

> **C++20 is mandatory.** SystemC's ABI is keyed per language standard
> (`sc_api_version_*_cxx202002L`). Mixing a C++20 SystemC with a consumer
> built under any other standard breaks linking with an undefined
> `sc_api_version_*` symbol. See `.cursor/rules/cpp20-build.mdc`.

---

## One-time setup: build `smc-vp`

`smc-vp` is the executable that runs the firmware in this directory. Build it
from the repo root once (the build is incremental afterwards):

```bash
# From the repo root.  The example uses an isolated bring-up build dir
# (vp/build_smc); the canonical location is vp/build.
SYSTEMC_HOME=/path/to/systemc-3.0.2-cxx20 \
CCI_HOME=/path/to/cci-cxx20 \
WHISPER_HOME=/path/to/whisper \
BOOST_DIR=/opt/homebrew/opt/boost \
cmake -S vp -B vp/build_smc -DCMAKE_BUILD_TYPE=Release

cmake --build vp/build_smc --target smc-vp -j
```

This produces `vp/build_smc/bin/smc-vp`. The `Makefile.common` here defaults
`VP=../../../vp/build/bin/smc-vp`; override with `VP=.../build_smc/bin/smc-vp`
during bring-up (or just build into `vp/build`).

Verify it runs:

```bash
vp/build_smc/bin/smc-vp   # prints "Usage: smc-vp <cci-ini> <elf> [sim_time_ms]"
```

---

## Run all tests automatically

`run_smc_vp_tests.sh` is a host-agnostic runner (macOS / RHEL / Ubuntu) that
auto-detects the RISC-V toolchain, the `smc-vp` executable, and the required
SystemC/CCI/Whisper/Boost install locations, then builds and runs the firmware
tests. If `smc-vp` is missing, the script builds it for you.

```bash
cd sw/smc-vp-tests

./run_smc_vp_tests.sh --list          # list available tests
./run_smc_vp_tests.sh                 # build + run all smc-* tests
./run_smc_vp_tests.sh smc-dma-test    # build + run a single test by name
./run_smc_vp_tests.sh -i              # choose a single test from a numbered menu
./run_smc_vp_tests.sh --build-vp      # (re)build smc-vp first, then run all
```

The script searches common install prefixes and `PATH` for the toolchain. If your
setup is non-standard, override detection with environment variables:

```bash
RISCV_PREFIX=riscv64-elf- \
SYSTEMC_HOME=/path/to/systemc-3.0.2-cxx20 \
CCI_HOME=/path/to/cci-cxx20 \
WHISPER_HOME=/path/to/whisper \
BOOST_DIR=/opt/homebrew/opt/boost \
./run_smc_vp_tests.sh
```

---

## Available Tests

| Directory | Description |
|-----------|-------------|
| `smc-aou-test/` | Always-On Unit register smoke on `smc-vp` |
| `smc-avsbus-test/` | AVSBus controller register smoke |
| `smc-beu-test/` | BEU register smoke: ENABLE reset, PLIC/LOCAL enable masks, PHYS_ADDR RO, CAUSE re-arm, per-core windows |
| `smc-beu-error-test/` | BEU error-injection + accrual + SW-ack + per-core isolation (own `.ini`) |
| `smc-dma-test/` | DMA scratchpad-to-scratchpad copy |
| `smc-i2c-loopback-test/` | I2C0 controller → I2C1 target loopback (write + read) |
| `smc-i3c-loopback-test/` | I3C0 controller → echo target loopback (write + read) |
| `smc-map-coherence-test/` | Modeled IPs at RTL bases; named stubs expose identity tokens |
| `smc-memory-zeroer-test/` | memory_zeroer CSR program + DMA zero-fill into scratchpad (own `.ini`) |
| `smc-octs-timer-test/` | octs_system_timer as PRIMARY: reset defaults, CTRL RAZ/WI, GPIO_ENABLE, PRESET, TIMER_START, RUNNING |
| `smc-octs-timer-secondary-test/` | Same timer strapped SECONDARY; TIMER_START leaves the counter parked (no `sync_load`) |
| `smc-pll-wrapper-test/` | pll_wrapper CGM/AWM lock after REG_UPDATE |
| `smc-pvt-wrap-test/` | PVT wrapper process-clock, voltage droop, temperature status |
| `smc-telemetry-test/` | Telemetry receiver STATUS/CTRL/INTR + optional ATB inject |
| `smc-wdt-test/` | SiFive TLWDT stage-1 KEY/CMP/IP/FEED + stage-2 WDT_TIMEOUT / RESET |

### Shared Support Code (`common/`)

| File | Purpose |
|------|---------|
| `start.S` | RV64 startup: parks harts 1..3, sets mtvec/sp, clears BSS, inits UART0, calls `main` |
| `link.ld` | Linker script: single RAM region at `0x80000000` (cluster fast-mem window) |
| `printf.c` | Bare-metal `printf` (`%s`/`%u`/`%x`/`%0Nx`/`%%`) over UART0 THR with LSR THRE polling |
| `smc_common.h` | SMC peripheral base addresses + register offsets for CLINT, PLIC, reset, cpu_ctrl, PVT wrapper, I2C, I3C, UART + `REG_READ/WRITE` helpers |

## How it works

`smc-vp` (built from `vp/platform/smc/`) instantiates the full SMC
platform — fabric, PLIC, CLINT, reset unit, boot ROM, scratchpad, cpu_ctrl,
DMA, PVT wrapper, I3C, 3x I2C, 4x UART, and the Whisper-backed CVA6 cluster — and runs a
bounded SystemC simulation:

```
smc-vp <cci-ini> <firmware.elf> [sim_time_ms]
```

The ELF entry point is read from the ELF header and preset as the cluster's
immutable `reset_pc` before construction. Firmware is linked at `0x80000000`
(the cluster's fast-mem window) and loaded via `cluster.load_elf()`; MMIO
accesses to the local alias aperture `[0xC000_0000, 0xC100_0000)` route
through the fabric to the modeled peripherals. After the run, `smc-vp` drains
UART0's TX debug buffer to stdout so the firmware's `printf` output is visible
(the SMC UART model buffers TX rather than emitting live).

---

## Running standalone SMC peripheral tests

Each SMC IP has its own self-checking C++ SystemC test bench, driven by a
per-IP `run_tests.sh`. These run **without** the CPU / VP — they drive the
peripheral's TLM socket directly from a testbench initiator. This is the
fastest loop for iterating on a single IP.

```bash
# Run one IP's tests (Release + ASan + Coverage are separate invocations):
cd smc/peripherals/uart
SYSTEMC_HOME=/path/to/systemc-3.0.2-cxx20 \
CCI_HOME=/path/to/cci-cxx20 \
./run_tests.sh                # incremental Release build + run
./run_tests.sh --asan         # AddressSanitizer build + run (isolated build_asan/)
./run_tests.sh --coverage     # source coverage build + report (isolated build_cov/)
./run_tests.sh --clean        # wipe build dir, then configure/build/run
```

Available per-IP runners (each has `run_tests.sh`):

`bootrom`, `clint`, `cpu_ctrl`, `i2c_controller`, `i3c_controller`, `plic`,
`reset_unit`, `scratchpad_ram`, `uart`.

### Run all SMC IPs at once

The orchestrator runs every SMC IP — peripherals plus `smc_fabric` (and
`cpu_cluster` when `WHISPER_HOME` + `BOOST_DIR` are set) — across the four
quality gates (Release, ASAN, Coverage, CTest) and writes a structured
summary into `smc/logs/`:

```bash
cd smc
SYSTEMC_HOME=/path/to/systemc-3.0.2-cxx20 \
CCI_HOME=/path/to/cci-cxx20 \
WHISPER_HOME=/path/to/whisper \
BOOST_DIR=/opt/homebrew/opt/boost \
./run_all_smc_tests.sh                  # all IPs, incremental
./run_all_smc_tests.sh --clean          # wipe build dirs first
./run_all_smc_tests.sh plic clint       # run only the listed IPs
```

Output lands in `smc/logs/<ip>/{release,asan,coverage,ctest}_run.log` and a
top-level `smc/logs/Full_result.log`.

---

## Running system tests

System tests exercise the **whole platform** end-to-end. There are two
flavours:

### Firmware-on-CPU tests on `smc-vp` (this directory)

These are the bare-metal RV64 tests here — the CPU fetches the firmware and
drives the peripherals through the fabric. This suite is the DV contract;
do not replace it with Zephyr. A parallel Zephyr port (`sw/zephyr-smc/`)
can run the same MMIO map as management firmware — see that README for
`mmio_poke` and how to port a test incrementally.

Build a test and run it on `smc-vp`:

```bash
cd sw/smc-vp-tests/smc-dma-test

# Build the ELF (override RISCV_PREFIX on Homebrew):
make RISCV_PREFIX=riscv64-elf-

# Run on smc-vp (point VP at your build tree if not vp/build_smc):
make sim RISCV_PREFIX=riscv64-elf- \
     VP=../../../vp/build_smc/bin/smc-vp
```

Expected UART verdict:

```
PASS: DMA scratchpad copy works
```

The same `make` / `make sim` pattern works from any present `smc-*-test/`
directory. To run every test, prefer `run_smc_vp_tests.sh`; if you need to
drive `make` directly:

```bash
cd sw/smc-vp-tests
for t in smc-*/; do make -C "$t" sim RISCV_PREFIX=riscv64-elf- \
    VP=../../../vp/build_smc/bin/smc-vp || break; done
```

A test **passes** when its UART output contains `PASS` and not `FAIL` (the
firmware prints the verdict). `make sim` runs a bounded simulation window
(`SIM_TIME_MS`, default 50 ms) then exits.

---

## Build and run a single test (quick reference)

```bash
cd smc-dma-test
make RISCV_PREFIX=riscv64-elf-                 # build the ELF
make sim  RISCV_PREFIX=riscv64-elf- \
          VP=../../../vp/build_smc/bin/smc-vp  # run on smc-vp
make dump RISCV_PREFIX=riscv64-elf-             # .dump (disassembly) + .sym
make clean                                      # remove artifacts
```

| Target | Description |
|--------|-------------|
| `make` / `make all` | Build the test ELF |
| `make sim` | Run on `smc-vp` (`SIM_TIME_MS` ms) |
| `make dump` | Generate `.dump` (disassembly) and `.sym` |
| `make clean` | Remove build artifacts |

## Environment Variables

| Variable | Default | Description |
|----------|---------|------------|
| `RISCV_PREFIX` | `riscv64-unknown-elf-` | Toolchain prefix |
| `VP` | `../../../vp/build/bin/smc-vp` | Path to the `smc-vp` executable |
| `VP_FLAGS` | `.../smc_platform_vp.ini` | CCI ini passed to `smc-vp` |
| `SIM_TIME_MS` | `50` | Simulation window in milliseconds |
| `EXTRA_CFLAGS` | _(empty)_ | Extra compiler flags |

## Writing a New Test

1. Create a directory under `smc-vp-tests/`.
2. Write `main.c` using `printf()` (from `common/printf.c`) and the
   `REG_READ`/`REG_WRITE` helpers in `common/smc_common.h`.
3. Add a `Makefile`:

```makefile
SRCS   = ../common/start.S main.c ../common/printf.c
OBJS   = $(SRCS:.c=.o)
OBJS  := $(OBJS:.S=.o)
TARGET = my_test_name
LDFLAGS += -T ../common/link.ld

include ../Makefile.common
```

4. Build and run:

```bash
make RISCV_PREFIX=riscv64-elf- && \
make sim RISCV_PREFIX=riscv64-elf- VP=../../../vp/build_smc/bin/smc-vp
```

### `printf` Format Specifiers

Same as `sw/sep-vp-tests`: `%s`, `%u`, `%x` (8 hex digits), `%0Nx` (N digits),
`%%`. **`%d` is not supported** — use `%u` / `%x`.

## Notes

- **Multi-hart parking**: `start.S` parks all harts except hart 0 (`mhartid != 0`
  spins in a `wfi` loop). Only hart 0 runs `main`, so test output is clean.
  `smc_platform` wires 4 harts (its IRQ vectors are sized to `NUM_HARTS=4`),
  so the cluster is always built with `num_harts=4`.
- **Determinism**: `smc_platform_vp.ini` / `smc-vp` defaults freeze CLINT
  `mtime` (`tick_period_ns=0`) so timer-driven behaviour is reproducible.
- **What's a "system" vs "standalone" test?** Standalone tests
  (`smc/peripherals/<ip>/run_tests.sh`) drive one IP's TLM socket directly
  from a C++ TB — no CPU, no fabric. System tests (this directory on `smc-vp`)
  instantiate the whole platform and let firmware drive traffic through the
  fabric to the peripherals.
