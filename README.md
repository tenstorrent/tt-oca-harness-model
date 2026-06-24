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
├── cmake/                         ← shared CMake helpers (FindSystemC, FindCCI)
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
│   │   └── uart_16550/            ← UART 16550
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
│   ├── 01_SMC_Architecture.md/.pdf
│   ├── 02_SMC_IP_LowLevel_Design.md/.pdf
│   ├── 03_SMC_Test_Plan.md/.pdf
│   ├── 04_SMC_VP_Exit_Criteria.md/.pdf
│   ├── component-developer-guide.md/.pdf
│   ├── maintainer-guide.md/.pdf
│   └── SystemC_Virtual_Platform_Customer_Guide.md/.pdf
├── scripts/
│   ├── md-to-pdf.sh               ← Markdown → PDF (pandoc + Chrome headless)
│   └── md-pdf.css
├── Makefile                       ← top-level: sep-vp, submodule-init, clean
├── RELEASE_NOTES.md
└── LICENSE
```

---

## Building the SEP VP

### Dependencies

- **CMake** 3.24+
- **C++ compiler**: GCC 9+ (C++17) or GCC 11+ (C++20)
- **SystemC** 3.0.2
- **CCI** 1.0.1
- **Boost** (`iostreams`, `program_options`, `log`)
- **OpenSSL** (for HMAC, KMAC, CSRNG crypto models)

### Installation

#### 1. System packages

```bash
sudo apt-get update
sudo apt install -y g++ make cmake autoconf \
    libboost-iostreams-dev libboost-program-options-dev libboost-log-dev \
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
> (marked `// TODO`) to the concrete iterator type. See the RELEASE_NOTES for
> the exact patch.

### Configure & Build

**Clone the repository:**

```bash
git clone git@github.com:tenstorrent/tt-oca-sim.git
cd tt-oca-sim
git submodule update --init --recursive
```

> **Note:** GitHub SSH keys must be configured — the CSML submodule uses SSH.

**Build:**

Edit **`vp/configure_vp.sh`** to set `SYSTEMC_HOME`, `CCI_HOME`, `BOOST_ROOT`,
and `OPENSSL_ROOT` to your install paths, then:

```bash
cd vp && ./configure_vp.sh && cd build && make sep-vp
```

Override on the command line:

```bash
CMAKE_BUILD_TYPE=Release CMAKE_CXX_STANDARD=20 cd vp && ./configure_vp.sh && cd build && make sep-vp
```

Or from the repo root:

```bash
SYSTEMC_HOME=/path/to/systemc make sep-vp
```

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

```bash
vp/build/bin/sep-vp vp/platform/sep/config/accellera_config.ini <firmware.elf>
```

### sep-vp-tests (Vayavya peripheral verification)

> Tests under `sw/sep-vp-tests/` verify the functional behavior of all modeled
> peripherals end-to-end from firmware running on the VeeR EL2 core.

```bash
cd sw/sep-vp-tests/sep-gpio-test
make        # build ELF
make sim    # run on VP
make debug  # run with GDB enabled
make gdb    # connect GDB (second terminal)
```

### Peripheral Model Unit Tests (SEP)

Each peripheral under `sep/peripherals/<ip>/` follows this layout:

```
<ip>/
├── CMakeLists.txt
├── README.md
├── run_tests.sh
├── include/
├── src/
├── test/
└── doc/
    ├── 01_<IP>_Specification/
    ├── 02_<IP>_HighLevel_Design.md
    └── 03_<IP>_Test_Plan.md
```

```bash
cd sep/peripherals/<ip>
./run_tests.sh              # build + run
./run_tests.sh --asan       # with AddressSanitizer
./run_tests.sh --coverage   # with lcov coverage report
./run_tests.sh --debug      # debug build
./run_tests.sh --ctest      # ctest
./run_tests.sh --docs       # doxygen docs
```

`sep/peripherals/run_all_peripherals.sh` builds and runs all SEP peripherals at once.
Logs are written to `sep/peripherals/logs/`.

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
bin/sep_fw_standalone.sh sim   aes_test     # run a built test on sep-vp
bin/sep_fw_standalone.sh run   aes_test     # build + run in one shot
```

### GDB debug workflow

Enable GDB in the INI: `och_sep_ss1.gdb : true`

```bash
bin/sep_fw_standalone.sh build-dbg aes_test   # build with -O0 -g

# Terminal 1: VP acts as GDB server
bin/sep_fw_standalone.sh debug aes_test        # default port 4000

# Terminal 2: connect gdb-multiarch
bin/sep_fw_standalone.sh gdb aes_test
```

### Using run_test.sh for fw/sep/tests

```bash
cd sw/tt-oca-hw-main/fw/sep/tests

./run_test.sh aes_sanity               # build + run
./run_test.sh aes_sanity --run-only    # run existing ELF
./run_test.sh aes_sanity --build-only  # build only
./run_test.sh aes_sanity -t 60         # custom timeout

./run_all_tests.sh                     # build + run all
./run_all_tests.sh --clean             # clean + build + run all
./run_all_tests.sh --no-build          # run only with existing ELFs
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
- `och_sep_cdns_spi_ctrl`, `och_sep_spi_mux_ctrl`

### Simulation aids (no hardware equivalent)

- **SIM_OUT bootcode virtual console** (`sep/peripherals/sep_virt_console`): the SEP
  bootcode reports status via `simput*` writes to `SEP_SCRATCH_COLD_SCRATCH_2`
  (`0x10802010`). The VP observes those writes through a write-tap on the `sep_scratch`
  stub, decodes them, and prints each line to the console as
  `[<time>] [INFO <v>] [SIM_OUT] - <text>`. This is an observability aid for the
  standalone-SEP configuration; it does not change the register's R/W semantics. In the
  SMU configuration the SMC reads the register itself, so the console can be disabled via
  `och_sep_ss1.sim_out.enable : false`.

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

---

## Documentation

| Document | Description |
|----------|-------------|
| `doc/01_SMC_Architecture.md` | SMC subsystem architecture (OCAH Ch. 6 modeling view) |
| `doc/02_SMC_IP_LowLevel_Design.md` | Low-level design for each SMC IP model |
| `doc/03_SMC_Test_Plan.md` | SMC model verification plan |
| `doc/04_SMC_VP_Exit_Criteria.md` | Exit criteria for SMC VP completion |
| `doc/component-developer-guide.md` | Day-to-day contributor workflow |
| `doc/maintainer-guide.md` | Repository maintenance guide |
| `doc/SystemC_Virtual_Platform_Customer_Guide.md` | Customer-facing VP usage guide |

PDF versions of all Markdown documents are generated with:

```bash
scripts/md-to-pdf.sh doc/
```
