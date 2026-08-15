# tt-oca-sim — Open Chiplet Atlas Virtual Platform

## Published Documentation

- **OCH Architecture and Implementation**: https://cuddly-barnacle-62pjzl3.pages.github.io/och/
- Source lives under `docs/och_source/`; published HTML lives under `docs/och/`.
- Rebuild locally with `docs/build_och_docs.sh`.

## What is Open Chiplet Atlas?

**Open Chiplet Atlas (OCA)** is Tenstorrent's chiplet-based System-in-Package (SiP)
architecture. It defines a standardized framework for building multi-chiplet systems
where multiple specialized silicon dies are co-packaged and communicate through a
common, well-specified set of interfaces and protocols.

The **Open Chiplet Atlas Harness (OCAH)** is the hardware specification that governs
every OCA-compliant chiplet. The "harness" is the infrastructure layer present in
every chiplet — common subsystems, AXI fabric topology, inter-chiplet protocols
(OCCP, OCTS), security boundaries, and management interfaces that all chiplets must
implement to be OCAH-compliant.

---

## SMC and SEP in the Open Chiplet Harness

Every OCA chiplet integrates two mandatory management subsystems defined in the OCAH
hardware specification:

### System Management Controller (SMC) — OCAH Ch. 6

The **SMC** is the per-chiplet management engine: a small, firmware-driven RISC-V
microcontroller cluster (1–4 Rocket RV64GC cores) that owns every aspect of
chiplet bring-up and runtime management. It is the "service processor" of the
chiplet, handling everything the compute fabric must not do itself:

| Function                    | Detail                                                             |
| --------------------------- | ------------------------------------------------------------------ |
| Clock & voltage management  | PLLs, AVS (Adaptive Voltage Scaling), power state transitions      |
| Reset management            | Cold / cool / FLR / watchdog reset trees                           |
| Hardware bring-up           | Boot ROM, eFuse/OTP, strap sampling                                |
| Inter-chiplet communication | 32-channel mailboxes, OCCP protocol, OCTS time-sync                |
| Security fabric             | Inbound/outbound AXI filters (×16), address remap, protection bits |
| System monitoring           | PVT sensors, telemetry (ATB sinks), log engine                     |
| Interrupt management        | PLIC (332 sources), CLINT, per-core WDTs, BEUs                     |
| Debug                       | RISC-V Debug Module, JTAG-to-AXI bridge                            |

In a multi-chiplet SiP the **primary** chiplet's SMC additionally orchestrates
secondary chiplets (reset sequencing, firmware distribution, telemetry aggregation)
via the OCCP protocol over I3C.

### Secure Enclave Processor (SEP) — OCAH Ch. SEP

The **SEP** is the per-chiplet security subsystem — an OpenTitan-derived secure
enclave with a RISC-V VeeR EL2 core. It is isolated from the main compute fabric
and communicates with the SMC exclusively through an AXI4 port and a dedicated
mailbox interface. The SEP is responsible for all security-sensitive operations
that must be isolated from untrusted software:

| Function              | Detail                                                                                |
| --------------------- | ------------------------------------------------------------------------------------- |
| Cryptographic engines | AES-256, HMAC-SHA-2, KMAC, OTBN (big-number co-processor), CSRNG, Entropy Source, EDN |
| Key management        | Key Manager, lifecycle-controlled key derivation, eFuse/OTP interface                 |
| Lifecycle control     | Lifecycle Controller (ROM_EXT → DEV → PROD → RMA states)                              |
| Secure DMA            | Isolated DMA with inbound/outbound filters                                            |
| Secure boot           | Verifies SMC firmware and compute firmware signatures                                 |
| Mailbox               | Host-facing and SMC-facing secure communication channels                              |
| AON timer             | Always-on watchdog and reset arbitration                                              |

The SMC provides the SEP with a dedicated AXI port (`sep_axi_in`) and shares
interrupt lines via mailbox doorbell IRQs. In the full chiplet, the SMC acts as
the trusted proxy through which the host interacts with the SEP.

---

## This Repository

`tt-oca-sim` provides **SystemC/TLM-2.0 virtual platform simulation** for both
OCAH subsystems:

| Subsystem | What is provided |
|-----------|-----------------|
| **SEP** | Full, runnable Virtual Platform (`sep-vp`) — models all SEP peripherals, runs actual RISC-V VeeR EL2 firmware, used for pre-silicon DV and firmware development |
| **SMC** | SystemC TLM-2.0 IP model library (PLIC, CLINT, CPU cluster, reset unit, bootrom, scratchpad, DMA, PVT wrapper, I3C, …) with per-IP unit tests, **plus a full runnable Virtual Platform (`smc-vp`)** that wires the fabric + every peripheral + the Whisper-backed CVA6 cluster and runs bare-metal RV64 firmware |

---

## Directory Structure

```
tt-oca-sim/
├── cmake/                         ← shared CMake helpers (FindSystemC, FindCCI, PeripheralCommon, …)
├── sep/                           ← SEP IP peripheral models
│   ├── peripherals/               ← individual IP models
│   │   ├── aes/                   ← AES-256 engine
│   │   ├── aon_timer/             ← Always-On timer / watchdog
│   │   ├── csrng/                 ← Cryptographically Secure RNG
│   │   ├── edn/                   ← Entropy Distribution Network
│   │   ├── efuse/                 ← eFuse/OTP controller
│   │   ├── el2_pic/               ← EL2 Platform-level Interrupt Controller (embedded in VeeR EL2 core)
│   │   ├── entropy_src/           ← Entropy Source
│   │   ├── hmac/                  ← HMAC-SHA-2 engine
│   │   ├── key_manager/           ← Key Manager (lifecycle-aware)
│   │   ├── kmac/                  ← KMAC / SHA-3 engine
│   │   ├── lifecycle_ctrl/        ← Lifecycle Controller
│   │   ├── local_master_alias_remap_ctrl/ ← Fixed local-master AXI alias remap
│   │   ├── mailbox/               ← Secure mailbox (host ↔ SEP)
│   │   ├── otbn/                  ← OpenTitan Big-Number co-processor
│   │   ├── secure_dma/            ← Isolated DMA engine
│   │   ├── sep_cpu_ctrl/          ← SEP CPU control/status registers
│   │   ├── sep_filter_ctrl/       ← Inbound/outbound AXI security filter
│   │   ├── sep_memory/            ← SRAM / ROM models
│   │   ├── sep_output_remap_ctrl/ ← AP/STEE output address remap
│   │   ├── sep_reset_ctrl/        ← Per-IP software reset control
│   │   ├── sep_scratch_cold/      ← Cold-domain scratch registers
│   │   ├── sep_scratch_warm/      ← Warm-domain scratch registers (stub, store-only)
│   │   ├── spi_controller/        ← SPI controller (OpenTitan)
│   │   ├── spi_flash/             ← SPI flash model (SFDP Profile 1)
│   │   ├── logs/                  ← per-peripheral run_all_peripherals.sh logs
│   │   ├── Coverage_Report.md     ← per-peripheral line/function coverage summary
│   │   ├── setup_build_env.sh     ← shared env for run_tests.sh / run_all_peripherals.sh
│   │   └── run_all_peripherals.sh ← batch peripheral tests (sources vp/configure_vp.sh)
│   ├── cpu/                       ← VeeR EL2 ISS + TLM-2.0 wrapper
│   └── utils/
│       ├── csml/                  ← Core SystemC Model Library (submodule — Vayavya CSML)
│       └── paged-memory/          ← PagedMemory header-only sparse storage engine
├── smc/                           ← SMC IP model library
│   ├── peripherals/               ← SMC peripheral models
│   │   ├── bootrom/               ← 64 KB Boot ROM
│   │   ├── clint/                 ← RISC-V CLINT (mtime, MSIP, MTIMECMP)
│   │   ├── i3c_controller/        ← MIPI I3C HCI v1.2 controller (×6 in HW)
│   │   ├── plic/                  ← RISC-V PLIC (332 interrupt sources)
│   │   ├── reset_unit/            ← Reset generation unit (cold / cool / FLR)
│   │   └── scratchpad_ram/        ← 1 MiB scratchpad SRAM (32 banks)
│   ├── cpu_cluster/               ← SMC CPU cluster (1–4 RV64GC, Whisper ISS)
│   ├── smc_fabric/                ← SMC AXI fabric / address router model
│   └── cmake/
│       └── SmcSystemCStd.cmake    ← auto-detects SystemC C++ standard
├── vp/                            ← Virtual Platforms
│   ├── configure_vp.sh            ← configure CMake + export build env (SEP)
│   ├── vp_build_env.sh            ← derived paths (BOOST_LIB, LD_LIBRARY_PATH, …)
│   ├── CMakeLists.txt
│   └── platform/
│       ├── infra/                 ← bus, PLIC, CLINT, ELF loader (SEP)
│       ├── sep/                   ← SEP platform wiring (och_sep_ss)
│       │   ├── main.cpp           ← sc_main entry point
│       │   ├── och_sep_ss.hpp     ← top-level SEP platform module
│       │   ├── inc/               ← SEP-specific headers (Args, memory map)
│       │   └── config/
│       │       ├── accellera_config.ini   ← CCI runtime parameters
│       │       └── veeriss_config.json    ← VeeR EL2 ISS configuration
│       └── smc/                   ← SMC platform wiring (smc-vp)
│           ├── main.cpp           ← sc_main entry point (CCI ini + ELF load + UART drain)
│           ├── smc_platform.hpp   ← top-level SMC platform module
│           ├── src/smc_platform.cpp
│           ├── inc/               ← helpers (addr_router, width_adapter, stub_target, …)
│           └── config/
│               └── smc_platform_vp.ini   ← CCI runtime parameters
├── sw/                            ← Firmware and DV tests
│   ├── sep-vp-tests/              ← Vayavya peripheral verification tests (SEP)
│   │   └── fw-tests-from-tt-oca-hw/   ← TT firmware test suite (fw/sep), self-contained
│   │       ├── fw/sep/tests/      ← the tests, plus run_all_tests.sh / run_test.sh
│   │       ├── fw/sep/bootcode/   ← SEP Boot ROM (BL0)
│   │       └── dependencies/      ← setup_dependencies.sh builds picolibc; also holds
│   │                                 meta/registers/c, the shared SEP register headers
│   ├── smc-vp-tests/              ← Bare-metal RV64 firmware tests (SMC, runs on smc-vp)
│   │   └── run_smc_vp_tests.sh    ← host-agnostic runner: auto-detects toolchain + smc-vp, builds/runs tests
│   └── zephyr-smc/                ← Out-of-tree Zephyr port for smc-vp (board/SoC + apps)
│       ├── zephyr_smc.sh          ← setup / build / run / test
│       └── apps/mmio_poke/        ← MMIO reachability of IPs with no Zephyr driver
├── doc/                           ← Architecture and design documentation
│   ├── component-developer-guide.md/.pdf
│   ├── maintainer-guide.md/.pdf
│   └── SystemC_Virtual_Platform_Customer_Guide.md/.pdf
├── Makefile                       ← top-level: sep-vp, smc-vp, submodule-init, clean
├── RELEASE_NOTES.md
├── LICENSE
└── LICENSE.riscv-vp-plusplus      ← upstream MIT license attribution
```

---

## Building the SEP VP

### Dependencies

- **CMake** 3.24+
- **C++ compiler**: GCC 9+ (C++17) or GCC 11+ (C++20, default)
- **SystemC** 3.0.2
- **CCI** 1.0.1
- **Boost** **1.84.0** (`iostreams`, `program_options`, `regex`)
- **OpenSSL** (for HMAC, KMAC, CSRNG crypto models) — tested on **macOS** and **RHEL** with **3.0.13**; on Ubuntu with **3.2.1** and **3.5.2**

### Installation

The OCH SEP VP has been tested on **Ubuntu**, **RHEL**, and **macOS**. The step-by-step installation examples below are for **Ubuntu** (`apt` packages and typical Linux install paths).

#### 1. System packages (Ubuntu)

```bash
sudo apt-get update
sudo apt install -y g++ make cmake autoconf \
    libboost-iostreams-dev libboost-program-options-dev libboost-regex-dev \
    libssl-dev libvncserver-dev doxygen graphviz
```

#### 2. SystemC 3.0.2

```bash
cd ~/Downloads
wget https://github.com/accellera-official/systemc/archive/refs/tags/3.0.2.tar.gz
tar zxvf 3.0.2.tar.gz && cd systemc-3.0.2
mkdir objdir && cd objdir
sudo mkdir -p /usr/local/systemc-3.0.2
touch ../docs/DEVELOPMENT.md   # workaround for known bug
../configure --prefix=/usr/local/systemc-3.0.2 CXXFLAGS="-std=c++20"
make && sudo make install
```

#### 3. RapidJSON (header-only, required by CCI)

```bash
cd ~/Downloads && git clone https://github.com/Tencent/rapidjson.git
```

#### 4. CCI 1.0.1

Download and extract (all builds):

```bash
cd ~/Downloads
wget https://github.com/accellera-official/cci/releases/download/v1.0.1/cci_v1.0.1.tar.gz
tar zxvf cci_v1.0.1.tar.gz && cd cci_v1.0.1
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

> **C++20 note:** CCI 1.0.1 requires a patch for C++20 builds (GCC 11+). In
> `src/cci/core/cci_value.h`, change both `typedef void value_type;` lines
> (marked `// TODO`) to the concrete iterator type. See below or RELEASE_NOTES for
> the exact patch.

Apply the patch from the CCI source root (`~/Downloads/cci_v1.0.1`), then continue with configure/install:

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

Then continue with configure/install:

### Configure & Build

**Clone the repository:**

```bash
git clone git@github.com:tenstorrent/tt-oca-sim.git
cd tt-oca-sim
git submodule update --init --recursive
```

**Build:**

1. Edit **`vp/configure_vp.sh`** and set `SYSTEMC_HOME`, `CCI_HOME`, `OPENSSL_ROOT`, and
   `BOOST_ROOT` to your install prefix (or rely on auto-discovery). Use a C++20 SystemC tree when
   `CMAKE_CXX_STANDARD=20`, the default.
2. Configure and build — no `source` step required; `./configure_vp.sh` passes all paths to
   CMake via `-D` flags and creates **`vp/build/`**:

```bash
unset BOOST_ROOT SYSTEMC_HOME CCI_HOME OPENSSL_ROOT
cd vp
./configure_vp.sh
cd build
make sep-vp
```

Override defaults on the command line (examples):

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

Optional: **`source vp/configure_vp.sh`** (from repo root) or **`source ./configure_vp.sh`**
(after `cd vp`) only if you need install paths exported in your shell (for example manual
`cmake` in a peripheral directory, or debugging). It is **not** required for the VP
configure/build steps above — use **`./configure_vp.sh`** there instead.

Output binary: `vp/build/bin/sep-vp`

**C++ standard note:** The `smc/cmake/SmcSystemCStd.cmake` helper probes the
linked SystemC library and automatically sets `CMAKE_CXX_STANDARD` to match.
Point `SYSTEMC_HOME` at the SystemC build compiled with the standard you intend
to use — mismatches fail at link time with an `sc_api_version_*` undefined symbol.

---

## Running the SEP VP

### Runtime Configuration

**`vp/platform/sep/config/accellera_config.ini`** — CCI parameters for all models
(verbosity, algorithm selection, eFuse values, etc.)

**`vp/platform/sep/config/veeriss_config.json`** — VeeR EL2 ISS configuration
(XLEN, NMI vector, ICCM/DCCM layout, RISC-V extensions, CSR overrides).
Referenced from the INI via `och_sep_ss1.configFile`.

```bash
vp/build/bin/sep-vp vp/platform/sep/config/accellera_config.ini <firmware.elf>
```

### sep-vp-tests (Vayavya peripheral verification)

> Tests under `sw/sep-vp-tests/` verify the functional behavior of all modeled
> peripherals end-to-end from firmware running on the VeeR EL2 core.

```bash
cd sw/sep-vp-tests/sep-hmac-test
make             # build ELF
make sim         # run on VP
make debug       # run with GDB enabled
make gdb         # connect GDB (second terminal)
```

### Peripheral Model Unit Tests (SEP)

Each peripheral under `sep/peripherals/<ip>/` follows this layout:

```
<ip>/
├── CMakeLists.txt
├── README.md
├── run_tests.sh          ← sources ../setup_build_env.sh (VP paths required)
├── include/              ← public headers
├── src/                  ← implementation
├── test/                 ← CTest-registered unit tests
└── docs/
    ├── 01_<IP>_Specification/
    ├── 02_<IP>_HighLevel_Design.md
    └── 03_<IP>_Test_Plan.md
```

#### Build environment (required)

Peripheral standalone builds use the **same install paths as the VP**: `SYSTEMC_HOME`, `CCI_HOME`,
`OPENSSL_ROOT`, `BOOST_ROOT`, and (for SystemC 3.x) a matching `CMAKE_CXX_STANDARD`.

Every `run_tests.sh` and `run_all_peripherals.sh` **automatically** loads these via
`sep/peripherals/setup_build_env.sh`, which quietly **`source`s `vp/configure_vp.sh`** and passes
the resulting paths to cmake as `CMAKE_EXTRA_ARGS` (`SystemCLanguage_DIR`, `SystemCCCI_DIR`,
OpenSSL/Boost roots, C++ standard, and stale-cache detection).

**You must configure paths before running peripheral tests.** Either:

1. **Edit `vp/configure_vp.sh`** — set the `:="${SYSTEMC_HOME:=…}"` defaults for your machine
   (same block used for the VP build), **or**
2. **Export the variables** in your shell or CI job before invoking tests, for example:

```bash
export SYSTEMC_HOME=/opt/systemc-3.0.1
export CCI_HOME=/opt/cci-1.0.2
export OPENSSL_ROOT=/usr                    # or a custom OpenSSL prefix
export BOOST_ROOT=/usr                      # or a custom Boost prefix
export CMAKE_CXX_STANDARD=17                # if SystemC was built with C++17
```

Pre-set environment variables override the defaults in `configure_vp.sh`. Match
`CMAKE_CXX_STANDARD` to the C++ standard your SystemC install was built with (mismatch causes
link errors such as missing `sc_api_version_*` symbols).

Manual `source vp/configure_vp.sh` is **not** required when using `run_tests.sh` or
`run_all_peripherals.sh` (they call `setup_build_env.sh` for you). Source it only for ad-hoc
cmake from a peripheral directory.

Single peripheral:

```bash
cd sep/peripherals/<ip>
./run_tests.sh                               # build + run (env loaded automatically)
./run_tests.sh --asan                        # with AddressSanitizer
./run_tests.sh --coverage                    # with lcov coverage report
./run_tests.sh --coverage --clean            # recommended on macOS (see note below)
./run_tests.sh --debug                       # debug build
./run_tests.sh --ctest                       # ctest
./run_tests.sh --docs                        # doxygen docs
```

All peripherals:

```bash
cd sep/peripherals
./run_all_peripherals.sh                     # release / asan / coverage / ctest for each model
./run_all_peripherals.sh --clean             # clean build dirs first
./run_all_peripherals.sh --clean aes hmac    # subset only
```

**Coverage on macOS:** Use `--clean` when running coverage (`./run_tests.sh --coverage --clean` or
`./run_all_peripherals.sh --clean`) so stale `.gcda` files from prior runs are removed before
capture. Incremental coverage builds on Apple Clang can leave corrupt profile data and report falsely
low percentages even when tests pass. Install `lcov` first on macOS (`brew install lcov`).

Logs from `run_all_peripherals.sh` are written to `sep/peripherals/logs/`.

### Peripheral Model Unit Tests (SMC)

```bash
cd smc/peripherals/<ip>
./run_tests.sh
```

The SMC CPU cluster test suite:

```bash
cd smc/cpu_cluster
./run_tests.sh
```

---

## Building the SMC VP

### Dependencies (in addition to the SEP VP deps above)

The SMC VP reuses SystemC, CCI, and Boost from the SEP build, and adds:

- **Whisper** — Tenstorrent's CVA6 Instruction Set Simulator, the CPU backend for
  `smc_cpu_cluster`. Build it against the same C++20 SystemC tree:

  ```bash
  git clone <whisper-repo> whisper && cd whisper
  SYSTEMC_HOME=/Users/pdroy/local/systemc-3.0.2-cxx20 \
  CCI_HOME=/Users/pdroy/local/cci-cxx20 \
  cmake -S . -B build -DCMAKE_CXX_STANDARD=20
  cmake --build build -j
  ```

  Point `WHISPER_HOME` (or the path used in `vp/platform/smc/CMakeLists.txt`)
  at the resulting build tree.

- **RISC-V toolchain** (RV64GC) for bare-metal firmware — same toolchain used for
  the TT firmware tests (see "Toolchain" below).

> **C++20 is mandatory** for `smc-vp`: the local SystemC/CCI installs are built with
> C++20 and the ABI is keyed per standard. See `.cursor/rules/cpp20-build.mdc`.

### Configure & Build

`smc-vp` is built from the same `vp/` CMake tree as `sep-vp`. After configuring the
VP (see "Building the SEP VP"), build the SMC target:

```bash
cd vp/build
make smc-vp
```

Output binary: `vp/build/bin/smc-vp` (path may differ by build dir; check
`vp/build/` for the executable).

Alternatively, configure a dedicated build tree so SEP and SMC caches don't
poison each other:

```bash
SYSTEMC_HOME=/Users/pdroy/local/systemc-3.0.2-cxx20 \
CCI_HOME=/Users/pdroy/local/cci-cxx20 \
cmake -S vp -B vp/build_smc -DSMC_CXX_STANDARD=20
cmake --build vp/build_smc --target smc-vp -j
```

---

## Running the SMC VP

### Runtime Configuration

**`vp/platform/smc/config/smc_platform_vp.ini`** — CCI parameters for the
SMC platform and CPU cluster (hart count, reset PC, fast-mem layout, model
verbosity, etc.).

```bash
vp/build/bin/smc-vp vp/platform/smc/config/smc_platform_vp.ini <firmware.elf>
```

`smc-vp` loads the ELF into the Whisper-backed CVA6 cluster fast-mem, sets
`reset_pc` to the ELF entry, runs the simulation, and finally drains UART0's TX
debug buffer to stdout so the firmware's `printf` output is visible on the console.

### smc-vp-tests (bare-metal RV64 firmware)

> Tests under `sw/smc-vp-tests/` are bare-metal RV64 firmware that exercise SMC
> peripherals (bootrom, CLINT, CPU control, DMA, I2C, I3C, PLIC, PVT wrapper,
> reset, scratchpad, UART, …) end-to-end from code running on the CVA6 cluster. See
> `sw/smc-vp-tests/README.md` for the full guide.

```bash
cd sw/smc-vp-tests/smc-uart-test
make             # build ELF (RV64 toolchain)
make sim         # build + run on smc-vp
```

Run the whole suite (the helper auto-detects the toolchain and `smc-vp`
binary, and builds `smc-vp` if it is missing):

```bash
cd sw/smc-vp-tests
./run_smc_vp_tests.sh              # run all smc-* tests
./run_smc_vp_tests.sh smc-uart-test # run a single test by name
./run_smc_vp_tests.sh -i            # choose a single test from a numbered menu
./run_smc_vp_tests.sh --build-vp     # rebuild smc-vp first, then run all
```

### Zephyr RTOS on smc-vp

Zephyr is the RTOS path for **SMC management firmware** (threads, timers,
shell, later real drivers).  It is not Linux and it is not a replacement for
`sw/smc-vp-tests/`.  The out-of-tree port lives in `sw/zephyr-smc/`; the
operator guide is `doc/zephyr-on-smc.adoc`.

```bash
cd sw/zephyr-smc
./zephyr_smc.sh setup            # once: clone Zephyr v4.3.0 + west update
./zephyr_smc.sh run hello        # boot banner on live UART0
./zephyr_smc.sh run shell        # uart:~$  (Tab / help — not bash)
./zephyr_smc.sh test poke        # MMIO poke of IPs that device list omits
```

Use `sw/zephyr-smc/config/smc_zephyr.ini` (ticking CLINT).  The default
`smc_platform_vp.ini` freezes `mtime` and Zephyr hangs waiting for a tick.

`device list` only names DTS + driver bindings (PLIC and UART0 today).  To
touch UART1, I2C, I3C, DMA, WDT, … use `apps/mmio_poke`, the shell `devmem`
command, or a `REG_READ`/`REG_WRITE` app against
`sw/smc-vp-tests/common/smc_common.h`.  Platform-level bare-metal tests can
be ported to Zephyr apps incrementally (same `smc-vp` load path); keep the
DV suite in `sw/smc-vp-tests/`.  Details: `sw/zephyr-smc/README.md`.

### Standalone SMC peripheral unit tests

Each SMC peripheral under `smc/peripherals/<ip>/` and the fabric/cluster have
their own C++ SystemC testbenches driven by `run_tests.sh` (Release / ASan /
Coverage as separate runs):

```bash
cd smc/peripherals/<ip>          # or smc/smc_fabric, smc/cpu_cluster
./run_tests.sh                   # release build + run
./run_tests.sh --asan            # AddressSanitizer + UBSan
./run_tests.sh --coverage        # ≥95% line coverage report
```

Run every SMC target in one go:

```bash
cd smc
./run_all_smc_tests.sh           # release / asan / coverage across all SMC targets
```

---

## Building and Running TT Firmware Tests

`sw/sep-vp-tests/fw-tests-from-tt-oca-hw/` — the SEP firmware tests written by the
TT RTL and firmware teams (`fw/sep` in the tt-oca-hw repo), migrated into this
repo so they build and run without a tt-oca-hw checkout. Running them alongside
`sw/sep-vp-tests/` means the VP is checked against software that was not written
with the VP in mind. That directory's own README covers the layout, what
`setup_dependencies.sh` builds and why, and the current pass/fail breakdown.

### Toolchain

**Debian / Ubuntu:**
```bash
sudo apt install gcc-riscv64-unknown-elf
```

**RHEL / TT internal machines:**
```bash
module load riscv-gnu-toolchain/2025.01.20-rhel-8.10
# or:
export RISCV_TOOLCHAIN_PATH=/tools_soc/opensrc/riscv-gnu-toolchain/2025.01.20-rhel-8.10
```

The scripts probe `PATH` for the toolchain themselves (`riscv64-unknown-elf-`,
`riscv64-elf-`, `riscv-none-elf-`, …), so `brew install riscv64-elf-gcc` also
works. Override with `GCC_PREFIX` or `RISCV_TOOLCHAIN_PATH`.

### Build and run tests

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

The first run also builds picolibc into `dependencies/`, which takes a few extra
minutes; later runs go straight to the tests. ELFs land in
`fw/sep/tests/<test_name>/<test_name>.elf` and per-test logs in
`fw/sep/tests/logs/`. `sep-vp` is located by walking up to the repo root, so it
only needs to have been built.

### GDB debug workflow

Enable GDB in the INI (`och_sep_ss1.gdb : true`), build the test with `-O0 -g`,
then run the VP directly on the ELF — it acts as a GDB server on port 4000 —
and attach `gdb-multiarch` from a second terminal.

### Building and running the SEP Boot ROM (SPI boot)

`sw/sep-vp-tests/fw-tests-from-tt-oca-hw/fw/sep/bootcode/` is the SEP Boot ROM (BL0) firmware — the code
that runs first out of reset, reads/validates a manifest from SPI flash, and hands
off to BL1. It has its own build (independent of `sep_fw_standalone.sh`) and its
own runtime wiring on the VP.

**Build the ROM ELF:**

```bash
cd sw/sep-vp-tests/fw-tests-from-tt-oca-hw/fw/sep/bootcode
make BOOT_SPI_CONTROLLER_OT=1 all
```

`BOOT_SPI_CONTROLLER_OT=1` is required — this selects the OpenTitan SPI host driver
(`sep_ot_spi.c`) at compile time instead of the default Cadence xSPI driver
(`sep_spi.c`, `BOOT_SPI_CONTROLLER_OT=0`). The VP only models the OpenTitan
`spi_controller`/`spi_flash` peripherals (see "Functional stubs" below for the
Cadence leg), so a ROM built with the default flag will never get past SPI init on
this VP. Output: `build/boot_rom.elf`, linked at ROM base (`0x10040000`).

The `non_secure_boot_spi`/`secure_boot_spi` Make targets (which pack a manifest +
BL1 payload into a flash image via `tt_boot_manifest`) need the private
`tt-boot-manifest` submodule and are **not** required for the steps below — the
checked-in `prebuilt/non_secure_boot.spi_preload` / `prebuilt/secure_boot.spi_preload`
already contain a manifest + `bl1_pass_test` payload and can be used directly.

**Stage the SPI flash image and select the boot strap**, in `accellera_config.ini`:

```ini
[bool]
# Primary chiplet + SPI boot (default is Secondary, which waits for an SMC
# that this VP does not model — the ROM will hang in BOOT_SECONDARY otherwise).
och_sep_ss1.smc.primary_chiplet : true

[string]
# Parsed directly into spi_flash's backing memory in start_of_simulation
# (Verilog $readmemh-style hex, "@addr" + hex byte pairs) — no .bin conversion
# needed. See "Simulation aids" below for the raw-binary alternative.
och_sep_ss1.spiPreload : ../../../../sw/sep-vp-tests/fw-tests-from-tt-oca-hw/fw/sep/bootcode/prebuilt/non_secure_boot.spi_preload
```

**Run**, overriding `targets` on the command line to point at the ROM ELF instead
of the ini's default firmware target:

```bash
vp/build/bin/sep-vp vp/platform/sep/config/accellera_config.ini \
    sw/sep-vp-tests/fw-tests-from-tt-oca-hw/fw/sep/bootcode/build/boot_rom.elf
```

A successful non-secure boot prints (via the `[SIM_OUT]`/`[SEP_STATUS]` consoles
described above) `BOOT_SPI` → `SPI_INIT_OK` → `MANIFEST_OK` → `BL1_FOUND` /
`BL1_COPIED` → `BL1_JUMP=0x10020000` → BL1's own `GO!`, ending in
`[VP] SIMULATION OF THE TEST PASSED`. The ROM parks the core in a `wfi` loop after
handoff (no self-terminating exit), so the run needs a manual `Ctrl-C` once `PASS`
appears.

### VP firmware test notes

**`sep_outbound_filter_init()`** has been commented out in all firmware tests under
`fw/sep/tests/`. This function initializes the SEP outbound filter which is not
modeled in the VP.

The following IPs are **not modeled** in the VP. Tests that exercise them will fail:

- `och_sep_cdns_spi_ctrl` (the Cadence SPI leg; the OpenTitan `och_sep_spi_mux_ctrl` is now a functional RW stub — see "Functional stubs" below)

### Simulation aids (no hardware equivalent)

- **SPI flash image backdoor-load** (`spi_flash` model, called from `start_of_simulation`): a
  simulation-only way to preload the SPI NOR flash model's contents. The VP loads a staged image
  from `data/flash_memory.bin` (relative to the run directory) so controller reads return the
  real manifest+payload images (two software banks) instead of erased `0xFF`. On silicon the
  flash is programmed by other means; this just stages that content for a run. When no image is
  staged it is a no-op that logs a "... not found — using blank (0xFF) memory" line and leaves
  the flash erased, so tests that stage nothing are unaffected. Alternatively, `och_sep_ss1.spiPreload`
  parses a Verilog `$readmemh`-style `.spi_preload` file directly into the same backing memory —
  see "Building and running the SEP Boot ROM (SPI boot)" above; `spiPreload` takes precedence
  when set, otherwise this raw-binary path is used.

### Functional stubs (simplified models of real hardware)

The hardware here **does** exist; the VP models the register interface but simplifies the
behavior behind it.

- **SPI mux control register stub** (`och_sep_ss.hpp`, backed by `sep_memory`): `OCH_SEP_SPI_MUX_CTRL`
  (`0x20001000`) is a real silicon register — the OpenTitan SPI driver's first action is a
  mux-select write to it. The VP maps it as a functional RW stub so the write does not fault, and
  it reads back the silicon reset default (`0x00000002`, `cs_force_high=1`) before any write. It
  stores and returns values only — it does **not** model SPI leg selection or forced chip-select,
  because the VP has a single hard-wired OpenTitan flash leg. Real mux/chip-select behavior is
  validated in RTL-level (UVM) verification.

### Troubleshooting

**Toolchain not found:**
```bash
export RISCV_TOOLCHAIN_PATH=/path/to/riscv-toolchain
```

**VP binary not found:**
```bash
make sep-vp   # build from repo root
```

**Build fails partway through setup:**
```bash
bin/sep_fw_standalone.sh venv      # Python venv only
bin/sep_fw_standalone.sh config    # VeeR EL2 config only
bin/sep_fw_standalone.sh picolibc  # picolibc only
```

---

## Common Configuration

### OTBN Algorithm

Selected via `algorithm_type` in `accellera_config.ini`

| Value        | Algorithm      |
| ------------ | -------------- |
| `otbn_loop`  | Loop algorithm |
| `smoke`      | Smoke test     |
| `p256_ecdsa` | P-256 ECDSA    |
| `rsa_3072`   | RSA 3072-bit   |
| `rsa_2048`   | RSA 2048-bit   |

### Controlling Verbosity

Each model's verbosity (0–5) via INI:

```ini
och_sep_ss1.hmac.verbosity    : 1
och_sep_ss1.otbn.verbosity    : 2
och_sep_ss1.sram.verbosity    : 0
```

### Log Files

| File                 | Generated by                   |
| -------------------- | ------------------------------ |
| `och_sep_ss.log`     | VP run (current directory)     |
| `veer_trace.log`     | VeeR ISS instruction trace     |
| `veer_inst_freq.log` | VeeR ISS instruction frequency |

Log file names can be changed via CCI parameters.

---

## GCC and C++ Compatibility

| CXX_STD | Compiler                  | SYSTEMC_API | Status        |
| ------- | ------------------------- | ----------- | ------------- |
| c++17   | gcc-toolset-9 (GCC 9.2)   | cxx201703L  | OK            |
| c++17   | system GCC 8.5            | cxx201703L  | OK            |
| c++20   | gcc-toolset-11 (GCC 11.2) | cxx202002L  | OK            |
| c++20   | system GCC 8.5            | cxx201709L  | Not Supported |
| c++20   | gcc-toolset-9 (GCC 9.2)   | cxx201709L  | Not Supported |

### Compiler Selection

Use a GCC version that supports the C++ standard selected via `CMAKE_CXX_STANDARD` in `vp/configure_vp.sh`.

On **RHEL 8**, GCC Toolsets can be used to select a newer compiler. For example, to build with GCC 11:

```bash
# Open a shell with GCC 11 on PATH
scl enable gcc-toolset-11 bash

unset BOOST_ROOT SYSTEMC_HOME CCI_HOME OPENSSL_ROOT

cd vp
CMAKE_CXX_STANDARD=20 ./configure_vp.sh
cd build && make sep-vp
```

Alternatively, point CMake to the desired compiler explicitly:

```bash
export PATH="/opt/rh/gcc-toolset-11/root/usr/bin:$PATH"
export CC=/opt/rh/gcc-toolset-11/root/usr/bin/gcc
export CXX=/opt/rh/gcc-toolset-11/root/usr/bin/g++
```

On **Ubuntu**, install the required compiler package (for example, `g++-11`) and either ensure it appears first on `PATH` or set `CC` and `CXX` before running `configure_vp.sh`:

```bash
export CC=gcc-11
export CXX=g++-11
```

Verify the active compiler:

```bash
g++ --version
```

Verify the compiler selected by CMake:

```bash
grep CMAKE_CXX_COMPILER build/CMakeCache.txt
```

---

## Documentation

| Document                                         | Description                     |
| ------------------------------------------------ | ------------------------------- |
| `doc/component-developer-guide.md`               | Day-to-day contributor workflow |
| `doc/maintainer-guide.md`                        | Repository maintenance guide    |
| `doc/SystemC_Virtual_Platform_Customer_Guide.md` | Customer-facing VP usage guide  |
