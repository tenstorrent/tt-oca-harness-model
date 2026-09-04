# tt-oca-sim — Open Chiplet Atlas Virtual Platform

The GitHub repository is
[`tenstorrent/tt-oca-harness-model`](https://github.com/tenstorrent/tt-oca-harness-model).
The project name used in documentation, binaries, and this tree is **tt-oca-sim**.

## Overview

`tt-oca-sim` is a SystemC/TLM-2.0 virtual platform for the **Open Chiplet
Atlas Harness (OCAH)**. It models the System Management Controller (SMC),
the Secure Enclave Processor (SEP), and the on-die SMU interconnect so
firmware and pre-silicon tests can run before silicon.

This repository includes Tenstorrent models built on
[riscv-vp-plusplus](https://github.com/ics-jku/riscv-vp-plusplus) (MIT).
Tenstorrent modifications and new models are licensed under Apache 2.0; see
[License](#license).

**Open Chiplet Atlas (OCA)** is Tenstorrent's chiplet-based System-in-Package (SiP)
architecture. It defines a standardized framework for building multi-chiplet systems
where multiple specialized silicon dies are co-packaged and communicate through a
common, well-specified set of interfaces and protocols.

The **Open Chiplet Atlas Harness (OCAH)** is the hardware specification that governs
every OCA-compliant chiplet. The "harness" is the infrastructure layer present in
every chiplet — common subsystems, AXI fabric topology, inter-chiplet protocols
(OCCP, OCTS), security boundaries, and management interfaces that all chiplets must
implement to be OCAH-compliant.

Architecture and register specification live in the hardware TRM
(`tt-oca-hw`). This repository documents the SystemC/TLM-2.0 implementation,
the test plan, and how to run tests.
Subsystem books: [`sep/doc/`](sep/doc/index.adoc),
[`smc/doc/`](smc/doc/index.adoc). IP example:
[`sep/peripherals/adams_bridge/doc/`](sep/peripherals/adams_bridge/doc/index.adoc).

Three runnable platforms ship from `vp/`:

| Binary | What it simulates |
|--------|-------------------|
| `sep-vp` | SEP only — VeeR EL2 firmware on the secure enclave |
| `smc-vp` | SMC only — Whisper-backed CVA6 cluster + SMC fabric and peripherals |
| `smu-vp` | SMC + SEP in one process, connected by the SMU on-die interconnect and AoU stub |

**Contents**

- [Getting Started](#getting-started)
- [Requirements](#requirements)
- [Installation](#installation)
- [Usage](#usage)
- [Options](#options)
- [Architecture](#architecture)
- [API Reference](#api-reference)
- [Deployment](#deployment)
- [Troubleshooting](#troubleshooting)
- [FAQ](#faq)
- [Contributing](#contributing)
- [License](#license)

## Getting Started

1. Install the host toolchain and libraries listed in [Requirements](#requirements)
   (CMake 3.24+, a C++20 compiler, SystemC 3.0.2, CCI 1.0.2, Boost, OpenSSL).
2. Clone this repository and initialize submodules — [Installation](#installation).
3. Build `sep-vp` — [Building the SEP VP](#building-the-sep-vp).
4. To run SMC or SMU platforms, also install the public
   [Whisper ISS](https://github.com/tenstorrent/whisper) and a RISC-V GNU
   toolchain, then build `smc-vp` / `smu-vp`.
5. Run firmware tests under `sw/sep-vp-tests/`, `sw/smc-vp-tests/`, and
   `sw/smu-vp-tests/`.

Minimal SEP bring-up after dependencies are installed:

```bash
git clone https://github.com/tenstorrent/tt-oca-harness-model.git
cd tt-oca-harness-model
git submodule update --init --recursive
cd vp
./configure_vp.sh
cd build && make sep-vp
./bin/sep-vp ../platform/sep/config/accellera_config.ini <firmware.elf>
```

## Requirements

Tested hosts: **Ubuntu 22.04 LTS**, **RHEL 8.10**, and **macOS** (Apple Clang).
See [RELEASE_NOTES.md](RELEASE_NOTES.md) for the compiler matrix used in the
current release.

| Dependency | Version | Used by | Notes |
|------------|---------|---------|-------|
| CMake | 3.24+ | All builds | 3.20 is enough for some SMC standalone trees |
| C++ compiler | GCC 11+ or Apple Clang | All builds | C++20 is the default and is **required** for `smc-vp` / `smu-vp` |
| [SystemC](https://github.com/accellera-official/systemc) | 3.0.2 | All models | Build with the same `-std=c++NN` you will use to compile the VP |
| [CCI](https://github.com/accellera-official/cci) | 1.0.2 (preferred) or 1.0.1 | All models | 1.0.2 bundles RapidJSON and is C++20-clean; 1.0.1 needs a patch |
| Boost | 1.84 (`iostreams`, `program_options`, `regex`) | VP + Whisper | Homebrew or a custom prefix |
| OpenSSL | 3.x | SEP crypto models | Tested: 3.0.13 (macOS/RHEL), 3.2.1 / 3.5.2 (Ubuntu). RHEL 8 system OpenSSL 1.1.1 is not enough for KMAC |
| [Whisper](https://github.com/tenstorrent/whisper) | public ISS | `smc-vp`, `smu-vp` | Build with `MEM_CALLBACKS=1` and C++20 |
| RISC-V GNU toolchain | GCC 11+, `riscv64-unknown-elf-` (or `riscv64-elf-`) | Firmware tests | SMU tests need both RV64 (SMC) and RV32 (SEP) |
| Python 3 | 3.x | Firmware / preload helpers | — |
| Git | with submodule support | Clone | CSML lives in `sep/utils/csml` |

Optional for docs and coverage:

- `doxygen`, `graphviz` — peripheral `--docs`
- `lcov` / `gcovr` — coverage reports (`brew install lcov` on macOS)
- `pandoc` and Chrome/Chromium — Markdown → PDF via `scripts/build_docs.sh`

C++ standard versus compiler:

| CXX_STD | Compiler | SYSTEMC_API | Status |
| ------- | -------- | ----------- | ------ |
| c++17 | gcc-toolset-9 (GCC 9.2) | cxx201703L | OK (SEP / Accellera flow) |
| c++17 | system GCC 8.5 | cxx201703L | OK (SEP / Accellera flow) |
| c++20 | gcc-toolset-11+ (GCC 11.2) | cxx202002L | OK (required for SMC/SMU) |
| c++20 | system GCC 8.5 | cxx201709L | Not supported |
| c++20 | gcc-toolset-9 (GCC 9.2) | cxx201709L | Not supported |

SystemC's ABI is keyed per language standard (`sc_api_version_*_cxx202002L`).
Point `SYSTEMC_HOME` at a SystemC tree built with the same standard as the VP.
Mismatches fail at link time with an undefined `sc_api_version_*` symbol.

## Installation

The platforms have been tested on **Ubuntu**, **RHEL**, and **macOS**. The
package commands below are for Ubuntu. On macOS, Homebrew supplies Boost,
OpenSSL, and (optionally) the RISC-V toolchain; `vp/configure_vp.sh`
auto-discovers common prefixes. On RHEL 8, enable a newer GCC first
(`scl enable gcc-toolset-12 bash`) — system GCC 8 does not support C++20.

### 1. System packages (Ubuntu)

```bash
sudo apt-get update
sudo apt install -y g++ make cmake autoconf \
    libboost-iostreams-dev libboost-program-options-dev libboost-regex-dev \
    libssl-dev libvncserver-dev doxygen graphviz
```

Firmware toolchain (Debian / Ubuntu):

```bash
sudo apt install gcc-riscv64-unknown-elf
```

Site or module-managed installs:

```bash
module load riscv-gnu-toolchain/<version>
# or:
export RISCV_TOOLCHAIN_PATH=/path/to/riscv-gnu-toolchain
```

The test runners probe `PATH` for `riscv64-unknown-elf-`, `riscv64-elf-`,
`riscv-none-elf-`, and similar prefixes (`brew install riscv64-elf-gcc` works).
Override with `GCC_PREFIX`, `RISCV_PREFIX`, or `RISCV_TOOLCHAIN_PATH`.

### 2. SystemC 3.0.2

```bash
cd ~/Downloads
wget https://github.com/accellera-official/systemc/archive/refs/tags/3.0.2.tar.gz
tar zxvf 3.0.2.tar.gz && cd systemc-3.0.2
mkdir objdir && cd objdir
sudo mkdir -p /usr/local/systemc-3.0.2
touch ../docs/DEVELOPMENT.md   # workaround for a known packaging bug
../configure --prefix=/usr/local/systemc-3.0.2 CXXFLAGS="-std=c++20"
make && sudo make install
```

Use a separate prefix (for example `/usr/local/systemc-3.0.2-cxx20`) if you
also keep a C++17 SystemC tree.

### 3. CCI 1.0.2 (recommended)

CCI 1.0.2 builds with CMake, bundles RapidJSON, and does not need the C++20
`value_type` patch required by 1.0.1. This is the version CI installs.

```bash
cd ~/Downloads
wget https://github.com/accellera-official/cci/releases/download/v1.0.2/cci_v1.0.2.tar.gz
tar zxvf cci_v1.0.2.tar.gz && cd cci_v1.0.2
cmake -S . -B build \
  -DCMAKE_CXX_STANDARD=20 \
  -DCMAKE_CXX_STANDARD_REQUIRED=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr/local/cci-1.0.2 \
  -DCMAKE_INSTALL_LIBDIR=lib \
  -DSYSTEMCCCI_BUILD_TESTS=OFF \
  -DBUILD_SHARED_LIBS=OFF
cmake --build build -j
sudo cmake --install build
```

Set `SYSTEMC_HOME` in the environment before this configure so CMake finds
SystemC, or pass `-DSystemCLanguage_DIR=...` if your install is non-standard.

#### CCI 1.0.1 fallback

If you must use CCI 1.0.1, clone RapidJSON and apply the C++20 iterator
patch before `./configure`:

```bash
cd ~/Downloads && git clone https://github.com/Tencent/rapidjson.git

cd ~/Downloads
wget https://github.com/accellera-official/cci/releases/download/v1.0.1/cci_v1.0.1.tar.gz
tar zxvf cci_v1.0.1.tar.gz && cd cci_v1.0.1
```

In `src/cci/core/cci_value.h`, change both `typedef void value_type;` lines
(marked `// TODO`) to the concrete iterator type:

```bash
cd ~/Downloads/cci_v1.0.1
patch -p1 <<'PATCH'
--- a/src/cci/core/cci_value.h
+++ b/src/cci/core/cci_value.h
@@ -764,7 +764,7 @@
   template<typename U> friend class cci_impl::value_iterator_impl;
   typedef cci_impl::value_ptr<cci_value_map_elem_cref> proxy_ptr;
 
-  typedef void value_type; // TODO: add  explicit value_type 
+  typedef cci_value_map_elem_cref value_type; // TODO: add  explicit value_type 
 public:
   typedef cci_value_map_elem_cref const_reference;
   typedef cci_value_map_elem_ref  reference;
@@ -791,7 +791,7 @@
 {
   template<typename U> friend class cci_impl::value_iterator_impl;
   typedef cci_impl::value_ptr<cci_value_map_elem_ref> proxy_ptr;
-  typedef void value_type; // TODO: add  explicit value_type
+  typedef cci_value_map_elem_ref value_type; // TODO: add  explicit value_type
 public:
   typedef cci_value_map_elem_cref const_reference;
   typedef cci_value_map_elem_ref  reference;
PATCH
```

Then:

```bash
sudo mkdir -p /usr/local/cci-1.0.1
mkdir objdir && cd objdir
export LD_LIBRARY_PATH=/usr/local/systemc-3.0.2/lib-linux64/
../configure \
  --with-systemc=/usr/local/systemc-3.0.2/ \
  --with-json=/home/$USER/Downloads/rapidjson/rapidjson \
  --prefix=/usr/local/cci-1.0.1 \
  CXXFLAGS="-std=c++20"
make && sudo make install
```

### 4. Clone this repository

```bash
git clone https://github.com/tenstorrent/tt-oca-harness-model.git
cd tt-oca-harness-model
git submodule update --init --recursive
```

SSH: `git clone git@github.com:tenstorrent/tt-oca-harness-model.git`

### 5. Whisper (SMC and SMU only)

`sep-vp` does not need Whisper. `smc-vp` and `smu-vp` use it as the CVA6
ISS backend.

```bash
git clone https://github.com/tenstorrent/whisper.git
cd whisper
make MEM_CALLBACKS=1 EXTRA_CXXFLAGS=-std=gnu++20
```

Alternatively, configure with CMake against the same C++20 SystemC/CCI trees
used for the VP. Point `WHISPER_HOME` at the Whisper **source** tree (the
directory that contains `GNUmakefile`). `run_tests.sh` under
`smc/cpu_cluster` will build Whisper on first run if the source is present.

Suggested layout (auto-detected without setting `WHISPER_HOME`):

```
parent/
├── tt-oca-harness-model/   ← this repo
└── whisper/                ← GNUmakefile here
```

CI validates against Whisper commit `a53d0f3e` (see `.github/workflows/ci.yml`).

### 6. Record install prefixes

`vp/configure_vp.sh` probes well-known prefixes. To pin locations:

```bash
export SYSTEMC_HOME=/usr/local/systemc-3.0.2
export CCI_HOME=/usr/local/cci-1.0.2
export OPENSSL_ROOT=/usr
export BOOST_ROOT=/usr
export CMAKE_CXX_STANDARD=20
# SMC / SMU:
export WHISPER_HOME=/path/to/whisper
export BOOST_DIR=/usr          # Whisper / smc-vp sometimes use BOOST_DIR
```

Per-standard variants (`SYSTEMC_HOME_C17` / `SYSTEMC_HOME_C20`, and the same
for CCI, Boost, and OpenSSL) let both toolchains live in one shell profile.
The set matching `CMAKE_CXX_STANDARD` wins over the unsuffixed variable.

Edit the `:="${SYSTEMC_HOME:=…}"` defaults in `vp/configure_vp.sh` if you
prefer machine-local defaults over environment variables.

## Usage

### Building the SEP VP

1. Optionally edit `vp/configure_vp.sh` and set `SYSTEMC_HOME`, `CCI_HOME`,
   `OPENSSL_ROOT`, and `BOOST_ROOT` (or rely on auto-discovery). Use a C++20
   SystemC tree when `CMAKE_CXX_STANDARD=20` (the script default).
2. Configure and build — no `source` step is required; `./configure_vp.sh`
   passes paths to CMake via `-D` flags and creates `vp/build/`:

```bash
unset BOOST_ROOT SYSTEMC_HOME CCI_HOME OPENSSL_ROOT
cd vp
./configure_vp.sh
cd build
make sep-vp
```

Override defaults on the command line:

```bash
unset BOOST_ROOT SYSTEMC_HOME CCI_HOME OPENSSL_ROOT
cd vp
CMAKE_BUILD_TYPE=Release CMAKE_CXX_STANDARD=20 ./configure_vp.sh
cd build && make sep-vp
```

Or from the repo root:

```bash
SYSTEMC_HOME=/path/to/systemc make sep-vp
```

`make sep-vp` at the repo root defaults `CMAKE_CXX_STANDARD` to **17** unless
you pass it or set `CXX_STD`. Prefer `vp/configure_vp.sh` (default **20**)
when you built SystemC as C++20.

Optional: `source vp/configure_vp.sh` only if you need install paths exported
in your shell (ad-hoc `cmake` in a peripheral directory, or debugging). It is
not required for the VP configure/build steps above.

Output binary: `vp/build/bin/sep-vp`

The `smc/cmake/SmcSystemCStd.cmake` helper probes the linked SystemC library
and sets `CMAKE_CXX_STANDARD` to match.

### Running the SEP VP

**`vp/platform/sep/config/accellera_config.ini`** — CCI parameters for all
models (verbosity, algorithm selection, eFuse values, and so on).

**`vp/platform/sep/config/veeriss_config.json`** — VeeR EL2 ISS configuration
(XLEN, NMI vector, ICCM/DCCM layout, RISC-V extensions, CSR overrides).
Referenced from the INI via `och_sep_ss1.configFile`.

```bash
vp/build/bin/sep-vp vp/platform/sep/config/accellera_config.ini <firmware.elf>
```

Usage: `sep-vp <cci-ini> [targets override]`

#### sep-vp-tests (Vayavya peripheral verification)

Tests under `sw/sep-vp-tests/` verify modeled peripherals end-to-end from
firmware running on the VeeR EL2 core. See [`sw/sep-vp-tests/README.md`](sw/sep-vp-tests/README.md).

```bash
cd sw/sep-vp-tests/sep-hmac-test
make             # build ELF
make sim         # run on VP
make debug       # run with GDB enabled
make gdb         # connect GDB (second terminal)
```

#### Building and running TT firmware tests

`sw/sep-vp-tests/fw-tests-from-tt-oca-hw/` is the SEP firmware suite written
by the TT RTL and firmware teams (`fw/sep` in `tt-oca-hw`), migrated here so
it builds without a `tt-oca-hw` checkout. That directory's README covers the
layout, what `setup_dependencies.sh` builds, and the current pass/fail
breakdown.

```bash
cd sw/sep-vp-tests/fw-tests-from-tt-oca-hw/fw/sep/tests

./run_all_tests.sh                          # build + run all, print a summary
./run_all_tests.sh --clean                  # clean + build + run all
./run_all_tests.sh --no-build               # run only with existing ELFs

./run_test.sh aes_sanity                    # build + run one
./run_test.sh aes_sanity --run-only         # run existing ELF
./run_test.sh aes_sanity --build-only       # build only
./run_test.sh aes_sanity -t 60              # custom timeout
```

The first run also builds picolibc into `dependencies/`, which takes a few
extra minutes. ELFs land in `fw/sep/tests/<test_name>/<test_name>.elf` and
per-test logs in `fw/sep/tests/logs/`. `sep-vp` is located by walking up to
the repo root.

##### GDB debug workflow

Enable GDB in the INI (`och_sep_ss1.gdb : true`), build the test with
`-O0 -g`, then run the VP directly on the ELF — it acts as a GDB server on
port 4000 — and attach `gdb-multiarch` from a second terminal.

##### SEP Boot ROM (SPI boot)

`sw/sep-vp-tests/fw-tests-from-tt-oca-hw/fw/sep/bootcode/` is the SEP Boot ROM
(BL0): the code that runs first out of reset, reads/validates a manifest from
SPI flash, and hands off to BL1.

```bash
cd sw/sep-vp-tests/fw-tests-from-tt-oca-hw/fw/sep/bootcode
make BOOT_SPI_CONTROLLER_OT=1 all
```

`BOOT_SPI_CONTROLLER_OT=1` selects the OpenTitan SPI host driver
(`sep_ot_spi.c`). The VP only models the OpenTitan `spi_controller` /
`spi_flash` peripherals; a ROM built with the default Cadence xSPI driver
will never get past SPI init. Output: `build/boot_rom.elf`, linked at ROM
base (`0x10040000`).

The `non_secure_boot_spi` / `secure_boot_spi` Make targets need the private
`tt-boot-manifest` submodule and are **not** required for the steps below —
the checked-in `prebuilt/non_secure_boot.spi_preload` /
`prebuilt/secure_boot.spi_preload` already contain a manifest +
`bl1_pass_test` payload.

Stage the SPI flash image and select the boot strap in
`accellera_config.ini`:

```ini
[bool]
# Primary chiplet + SPI boot (default is Secondary, which waits for an SMC
# that standalone sep-vp does not model — the ROM will hang in BOOT_SECONDARY).
och_sep_ss1.smc.primary_chiplet : true

[string]
och_sep_ss1.spiPreload : ../../../../sw/sep-vp-tests/fw-tests-from-tt-oca-hw/fw/sep/bootcode/prebuilt/non_secure_boot.spi_preload
```

```bash
vp/build/bin/sep-vp vp/platform/sep/config/accellera_config.ini \
    sw/sep-vp-tests/fw-tests-from-tt-oca-hw/fw/sep/bootcode/build/boot_rom.elf
```

A successful non-secure boot prints `BOOT_SPI` → `SPI_INIT_OK` →
`MANIFEST_OK` → `BL1_FOUND` / `BL1_COPIED` → `BL1_JUMP=0x10020000` → BL1's
`GO!`, ending in `[VP] SIMULATION OF THE TEST PASSED`. The ROM parks the
core in a `wfi` loop after handoff, so stop with `Ctrl-C` once `PASS`
appears.

##### VP firmware test notes

`sep_outbound_filter_init()` is commented out in firmware tests under
`fw/sep/tests/` — the SEP outbound filter is not modeled as a full RTL
equivalent in every path those tests expect.

The following IPs are **not modeled**. Tests that exercise them will fail:

- `och_sep_cdns_spi_ctrl` (Cadence SPI; the OpenTitan `och_sep_spi_mux_ctrl`
  is a functional RW stub — see [Functional stubs](#functional-stubs-simplified-models-of-real-hardware))
- Debug and Test Ports (DTP): JTAG / iJTAG / JTAG2AXI / cross-trigger. The
  VP uses ISS GDB for software debug.

##### Simulation aids (no hardware equivalent)

SPI flash image backdoor-load (`spi_flash` model, from
`start_of_simulation`): preload the NOR flash so controller reads return a
real manifest + payload instead of erased `0xFF`. Paths are resolved
relative to the `.ini` directory:

- `och_sep_ss1.spiPreload` — Verilog `$readmemh`-style `.spi_preload` text
- `och_sep_ss1.spiBackdoorFile` — raw binary

`spiPreload` wins when both are set. When neither is set the flash starts
erased. CCI requires JSON-quoted strings, for example
`och_sep_ss1.spiBackdoorFile : "data/my_image.bin"`.

##### Functional stubs (simplified models of real hardware)

- **SPI mux control** (`OCH_SEP_SPI_MUX_CTRL` at `0x20001000`): functional
  RW stub that reads back the silicon reset default (`0x00000002`,
  `cs_force_high=1`). It does **not** model SPI leg selection or forced
  chip-select; the VP has a single hard-wired OpenTitan flash leg.

### Building the SMC VP

C++20 is mandatory for `smc-vp`: the SystemC/CCI ABI is keyed per standard.

`smc-vp` is built from the same `vp/` CMake tree as `sep-vp`. After
configuring the VP and setting `WHISPER_HOME`:

```bash
cd vp/build
make smc-vp
```

Output binary: `vp/build/bin/smc-vp`

Dedicated build tree (avoids mixing SEP and SMC CMake caches):

```bash
SYSTEMC_HOME=/path/to/systemc-3.0.2-cxx20 \
CCI_HOME=/path/to/cci-cxx20 \
WHISPER_HOME=/path/to/whisper \
cmake -S vp -B vp/build_smc -DSMC_CXX_STANDARD=20 -DCMAKE_BUILD_TYPE=Release
cmake --build vp/build_smc --target smc-vp -j
```

### Running the SMC VP

**`vp/platform/smc/config/smc_platform_vp.ini`** — CCI parameters for the
SMC platform and CPU cluster (hart count, reset PC, fast-mem layout,
verbosity).

```bash
vp/build/bin/smc-vp vp/platform/smc/config/smc_platform_vp.ini <firmware.elf>
```

Usage: `smc-vp <cci-ini> <elf> [sim_time_ms] [--uart-live] [--uart-interactive]`

`smc-vp` loads the ELF into the Whisper-backed CVA6 cluster fast-mem, sets
`reset_pc` to the ELF entry, runs the simulation, and drains UART0's TX
debug buffer to stdout so firmware `printf` is visible.

#### smc-vp-tests (bare-metal RV64 firmware)

Tests under `sw/smc-vp-tests/` exercise SMC peripherals (bootrom, CLINT,
CPU control, DMA, I2C, I3C, PLIC, PVT wrapper, reset, scratchpad, UART, …)
from code running on the CVA6 cluster. Full guide:
[`sw/smc-vp-tests/README.md`](sw/smc-vp-tests/README.md).

```bash
cd sw/smc-vp-tests/smc-uart-test
make             # build ELF (RV64 toolchain)
make sim         # build + run on smc-vp
```

```bash
cd sw/smc-vp-tests
./run_smc_vp_tests.sh               # run all smc-* tests
./run_smc_vp_tests.sh smc-uart-test # run a single test by name
./run_smc_vp_tests.sh -i            # numbered menu
./run_smc_vp_tests.sh --build-vp    # rebuild smc-vp first, then run all
```

The helper auto-detects the toolchain and `smc-vp` binary, and builds
`smc-vp` if it is missing.

#### Zephyr RTOS on smc-vp

Zephyr is the RTOS path for **SMC management firmware** (threads, timers,
shell, later real drivers). It is not Linux and it is not a replacement for
`sw/smc-vp-tests/`. Out-of-tree port: [`sw/zephyr-smc/README.md`](sw/zephyr-smc/README.md).

```bash
cd sw/zephyr-smc
./zephyr_smc.sh setup            # once: clone Zephyr v4.3.0 + west update
./zephyr_smc.sh run hello        # boot banner on live UART0
./zephyr_smc.sh run shell        # uart:~$  (Tab / help — not bash)
./zephyr_smc.sh test poke        # MMIO poke of IPs that device list omits
```

Use `sw/zephyr-smc/config/smc_zephyr.ini` (ticking CLINT). The default
`smc_platform_vp.ini` freezes `mtime` and Zephyr hangs waiting for a tick.

`device list` only names DTS + driver bindings (PLIC and UART0 today). To
touch UART1, I2C, I3C, DMA, WDT, … use `apps/mmio_poke`, the shell `devmem`
command, or a `REG_READ` / `REG_WRITE` app against
`sw/smc-vp-tests/common/smc_common.h`.

### Building and running the SMU VP

`smu-vp` integrates SMC and SEP in one SystemC process over the SMU on-die
interconnect (RTL reference: `hw/smu/rtl/smu.sv` in `tt-oca-hw`). It exists
to run firmware that talks across the SMC↔SEP boundary and to carry
chiplet-facing AXI through the AoU LT stub. Details:
[`vp/platform/smu/docs/README.md`](vp/platform/smu/docs/README.md).

```bash
SYSTEMC_HOME=/path/to/systemc-3.0.2-cxx20 \
CCI_HOME=/path/to/cci-cxx20 \
WHISPER_HOME=/path/to/whisper \
cmake -S vp -B vp/build_smc -DCMAKE_BUILD_TYPE=Release
cmake --build vp/build_smc --target smu-vp -j
```

Output binary: `vp/build_smc/bin/smu-vp`

Whisper archives are linked into `smu-vp` as a shared library
(`libsmc_cluster_smu`) with Whisper symbols hidden, so the two WdRiscv forks
(Whisper for SMC, VeeR-ISS for SEP) can coexist in one executable.

```bash
smu-vp <smc-cci-ini> <smc-elf> <sep-cci-ini> <sep-elf> [sim_time_ms]
```

Default `sim_time_ms` is `50`. Example from
`sw/smu-vp-tests/smu-link-test/`:

```bash
../../../vp/build_smc/bin/smu-vp \
    ../../../vp/platform/smu/config/smc_smu_vp.ini smu_link_smc \
    ../../../vp/platform/smu/config/sep_smu_config.ini smu_link_sep 50
```

Firmware suite (RV64 SMC half + RV32 SEP half):

```bash
cd sw/smu-vp-tests
./run_smu_vp_tests.sh
./run_smu_vp_tests.sh smu-link-test
```

See [`sw/smu-vp-tests/README.md`](sw/smu-vp-tests/README.md). A test passes
when **both** firmware halves print PASS (SMC on UART0, SEP on the
virtconsole) and neither prints FAIL.

### Peripheral model unit tests

Each SEP peripheral under `sep/peripherals/<ip>/` follows this layout:

```
<ip>/
├── CMakeLists.txt
├── README.md
├── run_tests.sh          ← sources ../setup_build_env.sh (VP paths required)
├── include/              ← public headers
├── src/                  ← implementation
├── test/                 ← CTest-registered unit tests
└── doc/
    ├── implementation.adoc   # SystemC/TLM-2.0 model (not a TRM copy)
    └── test_plan.adoc        # cases + how to run
```

Standalone builds use the same install paths as the VP. Every `run_tests.sh`
and `run_all_peripherals.sh` loads them via
`sep/peripherals/setup_build_env.sh`, which sources `vp/configure_vp.sh`.

Release, AddressSanitizer, and coverage are **three separate builds**. Do
not pass `--asan` and `--coverage` together.

```bash
cd sep/peripherals/<ip>
./run_tests.sh                               # build + run
./run_tests.sh --asan                        # AddressSanitizer + UBSan
./run_tests.sh --coverage                    # coverage report
./run_tests.sh --coverage --clean            # recommended on macOS
./run_tests.sh --debug                       # debug build
./run_tests.sh --ctest                       # ctest
./run_tests.sh --docs                        # doxygen docs
```

```bash
cd sep/peripherals
./run_all_peripherals.sh                     # release / asan / coverage / ctest
./run_all_peripherals.sh --clean
./run_all_peripherals.sh --clean aes hmac    # subset
```

**Coverage on macOS:** use `--clean` so stale `.gcda` files are removed.
Incremental coverage builds on Apple Clang can leave corrupt profile data.
Install `lcov` first (`brew install lcov`). Logs from
`run_all_peripherals.sh` go to `sep/peripherals/logs/`.

SMC peripherals, fabric, CPU cluster, and AoU use the same `run_tests.sh`
pattern:

```bash
cd smc/peripherals/<ip>          # or smc/smc_fabric, smc/cpu_cluster, aou
./run_tests.sh
./run_tests.sh --asan
./run_tests.sh --coverage
```

```bash
cd smc
./run_all_smc_tests.sh           # release / asan / coverage across SMC targets
```

SMU interconnect unit tests:

```bash
cd vp/platform/smu
./run_tests.sh
```

`smc/cpu_cluster` is skipped in public CI unless `WHISPER_HOME` is available;
run it locally once Whisper is installed.

## Options

### Platform command lines

```text
sep-vp  <cci-ini> [targets override]
smc-vp  <cci-ini> <elf> [sim_time_ms] [--uart-live] [--uart-interactive]
smu-vp  <smc-cci-ini> <smc-elf> <sep-cci-ini> <sep-elf> [sim_time_ms]
```

| Flag / argument | Platform | Meaning |
|-----------------|----------|---------|
| `<cci-ini>` | all | Accellera CCI parameter file |
| `[targets override]` | `sep-vp` | Firmware ELF (or target list) instead of the INI default |
| `<elf>` | `smc-vp` | RV64 ELF loaded into cluster fast-mem |
| `[sim_time_ms]` | `smc-vp`, `smu-vp` | Wall of simulated time; default `50`. `0` with `--uart-interactive` runs until the user stops |
| `--uart-live` | `smc-vp` | Stream UART0 TX as it is produced |
| `--uart-interactive` | `smc-vp` | Live UART plus stdin → RX (implies `--uart-live`; default time `0` if unset) |

### Configure / build environment

Set these before `vp/configure_vp.sh`, `make`, or the test runners. All
prefix variables are optional — the configure script probes well-known
locations when they are unset.

| Variable | Default | Purpose |
|----------|---------|---------|
| `SYSTEMC_HOME` | auto-discovered | SystemC install prefix (`include/systemc.h`) |
| `CCI_HOME` | auto-discovered | CCI install prefix (`include/cci_configuration`) |
| `BOOST_ROOT` / `BOOST_DIR` | auto-discovered | Boost prefix |
| `OPENSSL_ROOT` | auto-discovered | OpenSSL prefix (`include/openssl/ssl.h`) |
| `CMAKE_BUILD_TYPE` | `Debug` (`configure_vp.sh`) | `Debug` or `Release` |
| `CMAKE_CXX_STANDARD` | `20` (`configure_vp.sh`); `17` (`make` at repo root) | Must match the SystemC ABI |
| `CXX_STD` | — | Alternate `c++17` / `c++20` spelling used by the top-level `Makefile` |
| `WHISPER_HOME` | auto-discovered next to the repo | Whisper source tree |
| `WHISPER_BUILD_DIR` | `build-<OS>` | Override Whisper build subdirectory |
| `RISCV_TOOLCHAIN_PATH` / `RISCV_PREFIX` / `GCC_PREFIX` | probed on `PATH` | Bare-metal RISC-V GNU toolchain |
| `VP` / `VP_BUILD_DIR` | `vp/build/bin/<platform>` | Override which VP binary a test runner uses |
| `SMC_INI` / `SEP_INI` / `SIM_TIME_MS` | SMU runner defaults | `run_smu_vp_tests.sh` overrides |

Per-standard aliases: `SYSTEMC_HOME_C17`, `SYSTEMC_HOME_C20`, `CCI_HOME_C17`,
`CCI_HOME_C20`, `BOOST_ROOT_C17`, `BOOST_ROOT_C20`, `OPENSSL_ROOT_C17`,
`OPENSSL_ROOT_C20`.

`configure_vp.sh` also accepts extra CMake arguments after `--`:

```bash
./configure_vp.sh -- -DFOO=bar
```

### CCI / INI knobs

SEP (`vp/platform/sep/config/accellera_config.ini`):

| Parameter | Role |
|-----------|------|
| `och_sep_ss1.configFile` | Path to `veeriss_config.json` |
| `och_sep_ss1.gdb` | `true` — GDB server on port 4000 |
| `och_sep_ss1.smc.primary_chiplet` | `true` for SPI/BL0 boot on standalone `sep-vp` |
| `och_sep_ss1.spiPreload` / `och_sep_ss1.spiBackdoorFile` | Flash image backdoor |
| `och_sep_ss1.<model>.verbosity` | Per-model verbosity `0`–`5` |
| `algorithm_type` | OTBN algorithm (see below) |

OTBN `algorithm_type` values:

| Value | Algorithm |
| ----- | --------- |
| `otbn_loop` | Loop algorithm |
| `smoke` | Smoke test |
| `p256_ecdsa` | P-256 ECDSA |
| `rsa_3072` | RSA 3072-bit |
| `rsa_2048` | RSA 2048-bit |

Verbosity example:

```ini
och_sep_ss1.hmac.verbosity    : 1
och_sep_ss1.otbn.verbosity    : 2
och_sep_ss1.sram.verbosity    : 0
```

SMC (`vp/platform/smc/config/smc_platform_vp.ini`): hart count, reset PC,
fast-mem layout, model verbosity. Zephyr needs
`sw/zephyr-smc/config/smc_zephyr.ini` so CLINT `mtime` ticks.

SMU applies integration presets after loading both INIs (SEP
`smc_global.forward_en`, matching `0x4000_0000` / `0x5000_0000` apertures).
See [`vp/platform/smu/docs/README.md`](vp/platform/smu/docs/README.md).

### Log files

| File | Generated by |
| ---- | ------------ |
| `och_sep_ss.log` | SEP VP run (current directory) |
| `veer_trace.log` | VeeR ISS instruction trace |
| `veer_inst_freq.log` | VeeR ISS instruction frequency |
| `sep/peripherals/logs/` | `run_all_peripherals.sh` |
| `fw/sep/tests/logs/` | TT firmware test runner |

Log file names can be changed via CCI parameters.

## Architecture

Every OCA chiplet integrates two mandatory management subsystems defined in
the OCAH hardware specification, plus the SMU that connects them.

### System Management Controller (SMC) — OCAH Ch. 6

The **SMC** is the per-chiplet management engine: a small, firmware-driven
RISC-V microcontroller cluster (1–4 Rocket / CVA6 RV64GC cores) that owns
chiplet bring-up and runtime management.

| Function | Detail |
| -------- | ------ |
| Clock & voltage management | PLLs, AVS (Adaptive Voltage Scaling), power state transitions |
| Reset management | Cold / cool / FLR / watchdog reset trees |
| Hardware bring-up | Boot ROM, eFuse/OTP, strap sampling |
| Inter-chiplet communication | 32-channel mailboxes, OCCP protocol, OCTS time-sync |
| Security fabric | Inbound/outbound AXI filters, address remap, protection bits |
| System monitoring | PVT sensors, telemetry (ATB sinks), log engine |
| Interrupt management | PLIC (332 sources), CLINT, per-core WDTs, BEUs |
| Debug | RISC-V Debug Module, JTAG-to-AXI bridge (DTP not modeled) |

In a multi-chiplet SiP the **primary** chiplet's SMC additionally
orchestrates secondary chiplets (reset sequencing, firmware distribution,
telemetry aggregation) via OCCP over I3C.

### Secure Enclave Processor (SEP) — OCAH Ch. SEP

The **SEP** is the per-chiplet security subsystem — an OpenTitan-derived
secure enclave with a RISC-V VeeR EL2 core. It is isolated from the main
compute fabric and communicates with the SMC through an AXI4 port and a
dedicated mailbox.

| Function | Detail |
| -------- | ------ |
| Cryptographic engines | AES-256, HMAC-SHA-2, KMAC, OTBN, Adams Bridge (ML-DSA-87 / ML-KEM-1024), CSRNG, Entropy Source, EDN |
| Key management | Key Manager, lifecycle-controlled key derivation, eFuse/OTP |
| Lifecycle control | Lifecycle Controller (ROM_EXT → DEV → PROD → RMA) |
| Secure DMA | Isolated DMA with inbound/outbound filters |
| Secure boot | Verifies SMC firmware and compute firmware signatures |
| Mailbox | Host-facing and SMC-facing secure communication channels |
| AON timer | Always-on watchdog and reset arbitration |

The SMC provides the SEP with a dedicated AXI port (`sep_axi_in`) and
shares interrupt lines via mailbox doorbell IRQs. In the full chiplet the
SMC is the trusted proxy through which the host interacts with the SEP.

### SMU on-die interconnect

`smu-vp` wires SMC and SEP as they sit in the SMU:

- **Dedicated SEP→SMC path** — SEP accesses to `[0x4000_0000, +2 MiB)`
  forward to the SMC fabric.
- **Crossbar paths** — `smu_axi_xbar` routes SMC↔SEP and die-to-die
  (`ext_in` / `ext_out`) traffic.
- **AoU stub** — chiplet-facing AXI through the AXI-over-UCIe loosely-timed
  model in `aou/`.

### What this repository provides

| Subsystem | What is provided |
|-----------|------------------|
| **SEP** | Full Virtual Platform (`sep-vp`) — all modeled SEP peripherals, VeeR EL2 firmware, pre-silicon DV |
| **SMC** | SystemC TLM-2.0 IP library plus a full Virtual Platform (`smc-vp`) — fabric, peripherals, Whisper-backed CVA6 cluster, bare-metal RV64 firmware and Zephyr |
| **SMU** | Combined platform (`smu-vp`) and dual-firmware tests that exercise the on-die link |
| **AoU** | Standalone AXI-over-UCIe LT model used by `smc-vp` / `smu-vp` |

Limitations (unmodeled or stubbed IP) are listed in
[RELEASE_NOTES.md](RELEASE_NOTES.md#current-limitations).

### Directory structure

```
tt-oca-sim/                    (GitHub: tenstorrent/tt-oca-harness-model)
├── cmake/                     shared CMake helpers (FindSystemC, FindCCI, …)
├── common/include/            shared register + logging helpers
│                              (reg_access.h, reg_map.h, sim_log.h)
├── sep/                       SEP IP peripheral models
│   ├── peripherals/           individual IP models + run_all_peripherals.sh
│   ├── cpu/                   VeeR EL2 ISS + TLM-2.0 wrapper
│   └── utils/
│       ├── csml/              Core SystemC Model Library (submodule)
│       └── paged-memory/      sparse storage engine
├── smc/                       SMC IP model library
│   ├── peripherals/           bootrom, clint, plic, uart, i2c, i3c, dma,
│   │                          reset_unit, scratchpad_ram, cpu_ctrl, wdt,
│   │                          beu, pvt_wrap, pll_wrapper, avsbus_controller,
│   │                          telemetry_receiver, memory_zeroer,
│   │                          octs_system_timer, …
│   ├── cpu_cluster/           1–4 RV64GC, Whisper ISS
│   ├── smc_fabric/            AXI fabric / address router
│   └── common/include/        canonical smc_axi_extension.h
├── aou/                       AXI-over-UCIe loosely-timed model
├── vp/                        Virtual Platforms
│   ├── configure_vp.sh        configure CMake + export build env
│   └── platform/
│       ├── infra/             bus, PLIC, CLINT, ELF loader (SEP)
│       ├── sep/               sep-vp (och_sep_ss)
│       ├── smc/               smc-vp
│       └── smu/               smu-vp: SMC + SEP + interconnect
├── sw/
│   ├── sep-vp-tests/          SEP firmware + Vayavya tests
│   ├── smc-vp-tests/          bare-metal RV64 SMC tests
│   ├── smu-vp-tests/          dual-firmware SMU tests
│   └── zephyr-smc/            out-of-tree Zephyr port for smc-vp
├── doc/                       customer, component, and maintainer guides
├── scripts/                   Markdown ↔ PDF helpers
├── Makefile                   sep-vp, smc-vp, submodule-init, clean
├── RELEASE_NOTES.md
├── LICENSE                    Apache 2.0 (software)
├── LICENSE-DOCS               CC-BY 4.0 (documentation and images)
├── LICENSE_understanding.txt
├── NOTICE                     third-party attributions
├── CONTRIBUTING.md
├── CODE_OF_CONDUCT.md
├── SECURITY.md
└── vp/LICENSE.riscv-vp-plusplus   upstream MIT (riscv-vp-plusplus)
```

Each modeled IP has a `README.md` plus `doc/index.adoc`,
`doc/implementation.adoc`, and `doc/test_plan.adoc`.

## API Reference

This repository is a SystemC/TLM model library and a set of VP executables,
not a packaged language SDK. The programming surfaces are:

| Surface | Where to start |
|---------|----------------|
| Customer usage (build, run, add an IP to a platform) | [`doc/SystemC_Virtual_Platform_Customer_Guide.md`](doc/SystemC_Virtual_Platform_Customer_Guide.md) |
| Writing / testing a new IP model | [`doc/component-developer-guide.md`](doc/component-developer-guide.md) |
| Maintaining the tree and releases | [`doc/maintainer-guide.md`](doc/maintainer-guide.md) |
| SEP book | [`sep/doc/index.adoc`](sep/doc/index.adoc) |
| SMC book | [`smc/doc/index.adoc`](smc/doc/index.adoc) |
| Per-IP model + test plan | `<subsystem>/peripherals/<ip>/doc/` |
| Register access helpers | [`common/include/reg_access.h`](common/include/reg_access.h), [`common/include/reg_map.h`](common/include/reg_map.h) |
| Transaction logging | [`common/include/sim_log.h`](common/include/sim_log.h) |
| Canonical SMC AXI TLM extension | [`smc/common/include/smc_axi_extension.h`](smc/common/include/smc_axi_extension.h) |
| Runtime knobs | Accellera CCI `cci::cci_param` in each module; preset from the platform INI |
| CSML-based SEP models | `csml_param` / `csml_reg` in `sep/utils/csml` |

Hand-written register models use `regmodel` (`apply_w1c`, `apply_woset`,
`Register32`, `RegisterMap`) rather than inline mask arithmetic. Runtime
configuration goes through CCI, not process-wide globals.

Firmware-facing register maps and programming sequences are specified in
the hardware TRM (`tt-oca-hw`), not duplicated here.

## Deployment

`tt-oca-sim` is a **local simulation toolchain**. There is no hosted service
and no supported Docker/cloud image in this repository. You deploy by
building the VP binaries on the machine that will run firmware.

| Method | How |
|--------|-----|
| Local developer machine | [Installation](#installation) + [Usage](#usage). Binaries land in `vp/build/bin/` (or `vp/build_smc/bin/`). |
| GitHub Actions CI | [`.github/workflows/ci.yml`](.github/workflows/ci.yml) builds SystemC/CCI, then `sep-vp`, SEP/SMC/SMU unit tests, `smc-vp` firmware, Zephyr, and `smu-vp`. RHEL 8 is covered by `.github/workflows/ci-rhel8.yml` (ASan skipped — the gcc-toolset ASan runtime is not packaged). |
| Isolated CMake trees | Use `vp/build` for SEP and `vp/build_smc` for SMC/SMU so caches do not poison each other. ASan and coverage always use `build_asan/` and `build_cov/` under the IP directory. |

CI caches SystemC 3.0.2 and CCI 1.0.2 keyed by OS + compiler + C++
standard. The `smc-vp` job uploads the binary for the Zephyr job. Whisper
is cloned from `https://github.com/tenstorrent/whisper.git` at the
`WHISPER_REV` pin in the workflow.

To convert Markdown design docs to PDF, use `scripts/build_docs.sh` — see
[`scripts/README.md`](scripts/README.md).

## Troubleshooting

**Undefined `sc_api_version_*` at link time.** The VP C++ standard does not
match the SystemC install. Rebuild SystemC with `-std=c++20` (or 17) and
set `CMAKE_CXX_STANDARD` to the same value. `vp/configure_vp.sh` defaults
to 20; `make sep-vp` at the repo root defaults to 17.

**Toolchain not found.**

```bash
export RISCV_TOOLCHAIN_PATH=/path/to/riscv-toolchain
# or:
export RISCV_PREFIX=riscv64-elf-    # Homebrew
```

**VP binary not found.**

```bash
make sep-vp    # or: cd vp/build && make smc-vp
```

**Whisper not found / `smc-vp` will not configure.** Set `WHISPER_HOME` to
the directory that contains `GNUmakefile`, or clone Whisper next to this
repo. Build with `MEM_CALLBACKS=1` and C++20.

**C++20 configure fails on RHEL 8.**

```bash
scl enable gcc-toolset-12 bash
unset BOOST_ROOT SYSTEMC_HOME CCI_HOME OPENSSL_ROOT
cd vp
CMAKE_CXX_STANDARD=20 ./configure_vp.sh
```

Or point CMake at the toolset compilers:

```bash
export PATH="/opt/rh/gcc-toolset-12/root/usr/bin:$PATH"
export CC=/opt/rh/gcc-toolset-12/root/usr/bin/gcc
export CXX=/opt/rh/gcc-toolset-12/root/usr/bin/g++
```

On Ubuntu, install `g++-11` (or newer) and set `CC` / `CXX` before
`configure_vp.sh`. Verify with `g++ --version` and
`grep CMAKE_CXX_COMPILER vp/build/CMakeCache.txt`.

**KMAC / OpenSSL errors on RHEL 8.** System OpenSSL is 1.1.1. Install
OpenSSL 3 (for example `~/local/openssl-3`) and set `OPENSSL_ROOT`.

**SEP firmware setup failed partway.**

```bash
bin/sep_fw_standalone.sh venv      # Python venv only
bin/sep_fw_standalone.sh config    # VeeR EL2 config only
bin/sep_fw_standalone.sh picolibc  # picolibc only
```

**SEP Boot ROM hangs in `BOOT_SECONDARY`.** Standalone `sep-vp` has no SMC
to wake a secondary chiplet. Set `och_sep_ss1.smc.primary_chiplet : true`.

**Zephyr hangs at boot.** Use `sw/zephyr-smc/config/smc_zephyr.ini`. The
default SMC INI freezes CLINT `mtime`.

**Coverage percentages look impossibly low on macOS.** Re-run with
`--coverage --clean`. Install `lcov` (`brew install lcov`).

**ASan log is non-empty but the binary exited 0.** `halt_on_error=0` keeps
the run going. Read `build_asan/asan.log.*` — a non-empty log fails the
gate. Do not disable LeakSanitizer to hide a leak.

**macOS-specific SEP link issues.** See
[`macOS_changes_README.md`](macOS_changes_README.md) for Apple Clang /
libc++ notes.

**Tests that touch unmodeled IP fail.** Expected for Cadence SPI, DTP/JTAG,
and other items in [RELEASE_NOTES.md](RELEASE_NOTES.md#current-limitations).

## FAQ

**What is the difference between `tt-oca-sim` and `tt-oca-harness-model`?**
They are the same project. The GitHub repository is
`tenstorrent/tt-oca-harness-model`. Documentation and binaries use the
name `tt-oca-sim`.

**Do I need `tt-oca-hw` to build or run the VP?**
No. Register maps and architecture live in that TRM, but this tree is
self-contained. TT firmware tests were imported under
`sw/sep-vp-tests/fw-tests-from-tt-oca-hw/`.

**Do I need Whisper to run SEP tests?**
No. Whisper is the SMC CVA6 backend. `sep-vp` uses the in-tree VeeR EL2
ISS.

**Can I use C++17?**
SEP / Accellera flows can still be built as C++17 if SystemC was built
that way. `smc-vp` and `smu-vp` require C++20. Prefer C++20 for new work.

**Why are ASan and coverage separate `run_tests.sh` invocations?**
The instrumentations conflict and produce misleading coverage if combined.
Each mode uses its own build directory (`build/`, `build_asan/`,
`build_cov/`).

**Is this cycle-accurate RTL?**
No. These are loosely-timed TLM-2.0 models for firmware bring-up and
pre-silicon software tests. Pin-level JTAG, scan, and some mux/filter
behaviors are stubbed or out of scope.

**How do I add a new peripheral?**
Follow [`doc/component-developer-guide.md`](doc/component-developer-guide.md)
and the customer guide. New IPs need a `test/` directory, `run_tests.sh`
(Release / ASan / coverage), an entry in the subsystem orchestrator, and a
CI matrix row.

**How are bugs and security issues reported?**
Functional bugs: [GitHub Issues](https://github.com/tenstorrent/tt-oca-harness-model/issues).
Vulnerabilities: [SECURITY.md](SECURITY.md) (private GitHub advisory, not
public issues).

**What license applies to documentation versus code?**
Software is Apache 2.0 ([LICENSE](LICENSE)). Documentation and images are
CC-BY 4.0 ([LICENSE-DOCS](LICENSE-DOCS)). Upstream `riscv-vp-plusplus`
remains MIT. See [License](#license).

## Documentation

| Document | Description |
| -------- | ----------- |
| [`doc/SystemC_Virtual_Platform_Customer_Guide.md`](doc/SystemC_Virtual_Platform_Customer_Guide.md) | Customer-facing VP usage and IP integration |
| [`doc/component-developer-guide.md`](doc/component-developer-guide.md) | How to develop and test IP models in this tree |
| [`doc/maintainer-guide.md`](doc/maintainer-guide.md) | Maintainer workflow and release notes process |
| [`RELEASE_NOTES.md`](RELEASE_NOTES.md) | Modeled IP list, coverage, limitations |
| [`sep/doc/index.adoc`](sep/doc/index.adoc) | SEP platform book |
| [`smc/doc/index.adoc`](smc/doc/index.adoc) | SMC platform book |
| [`vp/platform/smu/docs/README.md`](vp/platform/smu/docs/README.md) | SMU combined platform |
| [`sw/zephyr-smc/README.md`](sw/zephyr-smc/README.md) | Zephyr on `smc-vp` |
| [`scripts/README.md`](scripts/README.md) | Doc conversion helpers |
| [`CONTRIBUTING.md`](CONTRIBUTING.md) | Contribution process |
| [`SECURITY.md`](SECURITY.md) | Vulnerability reporting |
| [`CODE_OF_CONDUCT.md`](CODE_OF_CONDUCT.md) | Community standards |

## Contributing

We welcome contributions from the community:

- **Report bugs**: [GitHub Issues](https://github.com/tenstorrent/tt-oca-harness-model/issues)
- **Submit changes**: pull requests for bug fixes and new features
- **Review process**: pull requests are reviewed on a weekly basis

See [CONTRIBUTING.md](CONTRIBUTING.md) for coding standards, testing
requirements, and commit guidance. All contributors are expected to follow the
[Code of Conduct](CODE_OF_CONDUCT.md). Security issues go through
[SECURITY.md](SECURITY.md), not public issues.

## License

Overall license for this project, except where specified:
[LICENSE](LICENSE) (Apache License, Version 2.0).

License for all documentation and images only:
[LICENSE-DOCS](LICENSE-DOCS) (Creative Commons Attribution 4.0 International).

This repository includes a fork of
[riscv-vp-plusplus](https://github.com/ics-jku/riscv-vp-plusplus), which remains
under the MIT license; see [vp/LICENSE.riscv-vp-plusplus](vp/LICENSE.riscv-vp-plusplus).
Tenstorrent modifications and new models are Apache 2.0.

Third-party notices (VeeR ISS, SoftFloat, PQClean, OpenTitan, CSML, and others):
[NOTICE](NOTICE).

For the avoidance of doubt, see [LICENSE_understanding.txt](LICENSE_understanding.txt).
