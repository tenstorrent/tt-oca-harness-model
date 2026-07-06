# tt-oca-sim — Open Chiplet Atlas Virtual Platform

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

| Function | Detail |
|----------|--------|
| Clock & voltage management | PLLs, AVS (Adaptive Voltage Scaling), power state transitions |
| Reset management | Cold / cool / FLR / watchdog reset trees |
| Hardware bring-up | Boot ROM, eFuse/OTP, strap sampling |
| Inter-chiplet communication | 32-channel mailboxes, OCCP protocol, OCTS time-sync |
| Security fabric | Inbound/outbound AXI filters (×16), address remap, protection bits |
| System monitoring | PVT sensors, telemetry (ATB sinks), log engine |
| Interrupt management | PLIC (332 sources), CLINT, per-core WDTs, BEUs |
| Debug | RISC-V Debug Module, JTAG-to-AXI bridge |

In a multi-chiplet SiP the **primary** chiplet's SMC additionally orchestrates
secondary chiplets (reset sequencing, firmware distribution, telemetry aggregation)
via the OCCP protocol over I3C.

### Secure Enclave Processor (SEP) — OCAH Ch. SEP

The **SEP** is the per-chiplet security subsystem — an OpenTitan-derived secure
enclave with a RISC-V VeeR EL2 core. It is isolated from the main compute fabric
and communicates with the SMC exclusively through an AXI4 port and a dedicated
mailbox interface. The SEP is responsible for all security-sensitive operations
that must be isolated from untrusted software:

| Function | Detail |
|----------|--------|
| Cryptographic engines | AES-256, HMAC-SHA-2, KMAC, OTBN (big-number co-processor), CSRNG, Entropy Source, EDN |
| Key management | Key Manager, lifecycle-controlled key derivation, eFuse/OTP interface |
| Lifecycle control | Lifecycle Controller (ROM_EXT → DEV → PROD → RMA states) |
| Secure DMA | Isolated DMA with inbound/outbound filters |
| Secure boot | Verifies SMC firmware and compute firmware signatures |
| Mailbox | Host-facing and SMC-facing secure communication channels |
| AON timer | Always-on watchdog and reset arbitration |

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
| **SMC** | SystemC TLM-2.0 IP model library — individual models for each SMC peripheral (PLIC, CLINT, CPU cluster, reset unit, bootrom, scratchpad, I3C, …), with unit tests and documentation |

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
│   │   ├── entropy_src/           ← Entropy Source
│   │   ├── gpio/                  ← GPIO controller
│   │   ├── hmac/                  ← HMAC-SHA-2 engine
│   │   ├── key_manager/           ← Key Manager (lifecycle-aware)
│   │   ├── kmac/                  ← KMAC / SHA-3 engine
│   │   ├── lifecycle_ctrl/        ← Lifecycle Controller
│   │   ├── mailbox/               ← Secure mailbox (host ↔ SEP)
│   │   ├── otbn/                  ← OpenTitan Big-Number co-processor
│   │   ├── secure_dma/            ← Isolated DMA engine
│   │   ├── sep_memory/            ← SRAM / ROM models
│   │   ├── spi_controller/        ← SPI controller (OpenTitan)
│   │   ├── spi_flash/             ← SPI flash model (SFDP Profile 1)
│   │   ├── uart_16550/            ← UART 16550
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
│   └── cmake/
│       └── SmcSystemCStd.cmake    ← auto-detects SystemC C++ standard
├── vp/                            ← SEP Virtual Platform
│   ├── configure_vp.sh            ← configure CMake + export build env
│   ├── vp_build_env.sh            ← derived paths (BOOST_LIB, LD_LIBRARY_PATH, …)
│   ├── CMakeLists.txt
│   └── platform/
│       ├── infra/                 ← bus, PLIC, CLINT, ELF loader
│       └── sep/                   ← SEP platform wiring (och_sep_ss)
│           ├── main.cpp           ← sc_main entry point
│           ├── och_sep_ss.hpp     ← top-level SEP platform module
│           ├── inc/               ← SEP-specific headers (Args, memory map)
│           └── config/
│               ├── accellera_config.ini   ← CCI runtime parameters
│               └── veeriss_config.json    ← VeeR EL2 ISS configuration
├── sw/                            ← Firmware and DV tests
│   ├── sep-vp-tests/              ← Vayavya peripheral verification tests
│   └── tt-oca-hw-main/            ← TT DV + firmware test suites
│       ├── bin/sep_fw_standalone.sh
│       ├── dv/sep/tests/          ← DV test ELFs
│       └── fw/sep/tests/          ← Firmware test suite
├── doc/                           ← Architecture and design documentation
│   ├── component-developer-guide.md/.pdf
│   ├── maintainer-guide.md/.pdf
│   └── SystemC_Virtual_Platform_Customer_Guide.md/.pdf
├── Makefile                       ← top-level: sep-vp, submodule-init, clean
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

> **Note:** GitHub SSH keys must be configured — the CSML submodule uses SSH.

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
cd sw/sep-vp-tests/sep-gpio-test
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

## Building and Running TT Firmware Tests

`sw/tt-oca-hw-main/dv/sep/tests/` — DV tests from the TT hardware repository,
verified on the SEP VP.

`sw/tt-oca-hw-main/fw/sep/tests/` — Main firmware test suite verified on the SEP VP.

TT tests are managed by `bin/sep_fw_standalone.sh`.

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

**Bundled zip archive:**
```bash
cd sw/tt-oca-hw-main/toolchain/
unzip ../riscv-gnu-toolchain-2025.01.20-rhel-8.10.zip
# sep_fw_standalone.sh auto-detects it
```

> `sw/tt-oca-hw-main/bin/setup_sep_test_env.sh` is TT's internal RHEL/UVM script —
> not for VP firmware testing. Use `sep_fw_standalone.sh` instead.

### One-time setup

```bash
cd sw/tt-oca-hw-main
bin/sep_fw_standalone.sh setup
```

### Build tests

```bash
bin/sep_fw_standalone.sh build-all          # build all dv/sep/tests/
bin/sep_fw_standalone.sh build hello_world  # build a single test
bin/sep_fw_standalone.sh build-dbg aes_test # build with debug flags (-O0 -g)
```

ELFs: `sw/tt-oca-hw-main/dv/sep/tests/<test_name>/<test_name>.elf`

### Run tests

```bash
bin/sep_fw_standalone.sh sim   aes_test  # run a built test on sep-vp
bin/sep_fw_standalone.sh run   aes_test  # build + run in one shot
```

### GDB debug workflow

Enable GDB in the INI: `och_sep_ss1.gdb : true`

```bash
bin/sep_fw_standalone.sh build-dbg aes_test  # build with -O0 -g

# Terminal 1: VP acts as GDB server
bin/sep_fw_standalone.sh debug aes_test      # default port 4000

# Terminal 2: connect gdb-multiarch
bin/sep_fw_standalone.sh gdb aes_test
```

### Using run_test.sh for fw/sep/tests

```bash
cd sw/tt-oca-hw-main/fw/sep/tests

./run_test.sh aes_sanity                    # build + run
./run_test.sh aes_sanity --run-only         # run existing ELF
./run_test.sh aes_sanity --build-only       # build only
./run_test.sh aes_sanity -t 60              # custom timeout

./run_all_tests.sh                          # build + run all
./run_all_tests.sh --clean                  # clean + build + run all
./run_all_tests.sh --no-build               # run only with existing ELFs
```

### Linker script for dv/sep/tests
The original `dv/sep/tests/common/exec_from_tcms.ld` used outdated RTL simulation
addresses (`ITCM=0x01000000`, `DTCM=0x0`). TT advised that the correct reference is
`fw/sep/tests/common/exec_from_tcms.ld` which uses VP addresses (`ITCM=0xC0000000`,
`DTCM=0xC0040000`). Vayavya created `dv/sep/tests/common/exec_from_tcms_vp.ld` as a
copy; all `dv/sep/tests/` link against it via `common.mk`.

### VP firmware test notes

**`sep_outbound_filter_init()`** has been commented out in all firmware tests under
`fw/sep/tests/`. This function initializes the SEP outbound filter which is not
modeled in the VP.

The following IPs are **not modeled** in the VP. Tests that exercise them will fail:

- `PIC` (Platform Interrupt Controller)
- `sep_cpu_ctrl`, `sep_reset_ctrl`
- `local_master_alias_remap_ctrl`
- `och_sep_cdns_spi_ctrl` (the Cadence SPI leg; the OpenTitan `och_sep_spi_mux_ctrl` is now a functional RW stub — see "Functional stubs" below)

### Simulation aids (no hardware equivalent)

- **SIM_OUT bootcode virtual console** (`sep/peripherals/sep_virt_console`): the SEP
  bootcode reports status via `simput*` writes to `SEP_SCRATCH_COLD_SCRATCH_2`
  (`0x10802010`). The VP observes those writes through a write-tap on the `sep_scratch`
  stub, decodes them, and prints each line to the console as
  `[<time>] [INFO <v>] [SIM_OUT] - <text>`. This is an observability aid for the
  standalone-SEP configuration; it does not change the register's R/W semantics. In the
  SMU configuration the SMC reads the register itself, so the console can be disabled via
  `och_sep_ss1.sim_out.enable : false`.

- **SEP_STATUS production-status console** (`sep/peripherals/sep_status_report`): the SEP
  bootcode also reports status on a *production* path — `report_status()` pushes encoded
  32-bit status codes into a ring buffer in SMC SRAM that the SMC reads on silicon. The VP
  observes those ring writes through an observation-only write-tap on the `smc_global` stub,
  decodes each code (severity, firmware stage, value, and a symbolic `SEP_MSG_*` name
  resolved at run time from `och_sep_ss1.sep_status.names_tsv`), and prints each as
  `[<time>] [INFO <v>] [SEP_STATUS] - <stage> <SEVERITY> 0xVVVV <NAME>`. It never modifies
  the ring or advances the consumer (`tail`) pointer, so it does not change firmware-observable
  behavior. Disable via `och_sep_ss1.sep_status.enable : false` (e.g. once an SMC-emulation
  model consumes the ring itself).

- **SPI flash image backdoor-load** (`spi_flash` model, called from `start_of_simulation`): a
  simulation-only way to preload the SPI NOR flash model's contents. The VP loads a staged image
  from `data/flash_memory.bin` (relative to the run directory) so controller reads return the
  real manifest+payload images (two software banks) instead of erased `0xFF`. On silicon the
  flash is programmed by other means; this just stages that content for a run. When no image is
  staged it is a no-op that logs a "... not found — using blank (0xFF) memory" line and leaves
  the flash erased, so tests that stage nothing are unaffected.


### Tests added by Vayavya

The following tests under `sw/tt-oca-hw-main/dv/sep/tests/` were added by Vayavya:

| Test | Purpose |
|------|---------|
| `interrupt_test` | Verifies PLIC/CLINT interrupt controller behavior — triggers MSIP via CLINT, timer interrupt (MTIP), and synchronous exception; confirms the VeeR EL2 trap handler dispatches and returns correctly |
| `wdog_reset_test` | Verifies watchdog-triggered system reset — arms the AON timer watchdog bite threshold, waits for the bite to fire, and confirms the VP issues a full system reset and that the VeeR ISS restarts from the entry point with SRAM contents preserved across the reset |

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

| Value | Algorithm |
|-------|-----------|
| `otbn_loop` | Loop algorithm |
| `smoke` | Smoke test |
| `p256_ecdsa` | P-256 ECDSA |
| `rsa_3072` | RSA 3072-bit |
| `rsa_2048` | RSA 2048-bit |

### Controlling Verbosity

Each model's verbosity (0–5) via INI:

```ini
och_sep_ss1.hmac.verbosity    : 1
och_sep_ss1.otbn.verbosity    : 2
och_sep_ss1.sram.verbosity    : 0
```

### Log Files

| File | Generated by |
|------|-------------|
| `och_sep_ss.log` | VP run (current directory) |
| `veer_trace.log` | VeeR ISS instruction trace |
| `veer_inst_freq.log` | VeeR ISS instruction frequency |

Log file names can be changed via CCI parameters.

---

## GCC and C++ Compatibility

| CXX_STD | Compiler | SYSTEMC_API | Status |
|---------|----------|-------------|--------|
| c++17 | gcc-toolset-9 (GCC 9.2) | cxx201703L | OK |
| c++17 | system GCC 8.5 | cxx201703L | OK |
| c++20 | gcc-toolset-11 (GCC 11.2) | cxx202002L | OK |
| c++20 | system GCC 8.5 | cxx201709L | Not Supported |
| c++20 | gcc-toolset-9 (GCC 9.2) | cxx201709L | Not Supported |

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

| Document | Description |
|----------|-------------|
| `doc/component-developer-guide.md` | Day-to-day contributor workflow |
| `doc/maintainer-guide.md` | Repository maintenance guide |
| `doc/SystemC_Virtual_Platform_Customer_Guide.md` | Customer-facing VP usage guide |
```
