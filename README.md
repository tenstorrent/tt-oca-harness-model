# Tenstorrent SEP

## Overview

SystemC/TLM-based Virtual Platform (VP) for the Tenstorrent OCH SEP (Secure Enclave Processor). Models OpenTitan-derived IP peripherals and integrates a RISC-V VeeR EL2 core via a custom TLM wrapper.

The platform supports two SystemC backends:
- **Accellera** — standalone `sep-vp` executable

---

## Directory Structure

```
tenstorrent_sep/
├── cmake/                        ← shared CMake modules (FindSystemC, FindCCI, etc.)
├── sep/                          ← all SEP IP peripheral models
│   ├── peripherals/              ← individual IP models (aes, hmac, uart, otbn, …)
│   ├── cpu/                      ← VeeR EL2 ISS + TLM wrapper
│   └── utils/
│       ├── csml/                 ← Core SystemC Model Library (Registers modelling, CCI params, logging)
│       └── paged-memory/         ← PagedMemory header-only sparse storage engine
├── vp/                           ← VP platform (wires all models into a complete VP)
│   ├── configure_vp.sh           ← configure CMake + export build env
│   ├── CMakeLists.txt
│   └── platform/
│       ├── infra/                ← RISC-V VP infrastructure (bus, PLIC, CLINT, ELF loader)
│       └── sep/                  ← SEP platform wiring (och_sep_ss)
│           ├── main.cpp          ← sc_main entry point
│           ├── och_sep_ss.hpp    ← top-level platform module
│           ├── inc/              ← SEP-specific headers (Args, memory map, mailbox)
│           ├── config/           ← runtime config files (.ini, .json)
├── sw/                           ← firmware and software tests
│   ├── sep-vp-tests/             ← standalone VP firmware tests (gpio, hmac, spi, …)
│   └── tt-oca-hw-main/           ← TT firmware tests and build infrastructure
│       ├── bin/sep_fw_standalone.sh  ← build + run script for TT tests
│       └── fw/sep/tests/         ← TT test sources + run_test.sh / run_all_tests.sh
├── Makefile                      ← convenience shortcut for building the VP
├── LICENSE
└── LICENSE.riscv-vp-plusplus     ← upstream MIT license attribution
```

---

## Flow

### Dependencies

- **CMake** 3.24+
- **C++ compiler** with C++17 (GCC 9+) or C++20 (GCC 11+)
- **SystemC** 3.0.1
- **CCI** 1.0.1
- **Boost** (`iostreams`, `program_options`, `log`)
- **OpenSSL** (for HMAC, KMAC, CSRNG crypto models) — tested on Ubuntu with **3.2.1** and **3.5.2**; on RHEL with **3.0.13**

### Installation

#### 1. System packages

```bash
sudo apt-get update
sudo apt install -y g++ make cmake autoconf \
    libboost-iostreams-dev libboost-program-options-dev libboost-log-dev \
    libssl-dev libvncserver-dev doxygen graphviz
```

#### 2. SystemC 3.0.1

```bash
cd ~/Downloads
wget https://github.com/accellera-official/systemc/archive/refs/tags/3.0.1.tar.gz
tar zxvf 3.0.1.tar.gz && cd systemc-3.0.1
mkdir objdir && cd objdir
sudo mkdir -p /usr/lib/systemc-3.0.1
touch ../docs/DEVELOPMENT.md   # workaround for known bug
../configure --prefix=/usr/lib/systemc-3.0.1
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
sudo mkdir -p /usr/lib/cci-1.0.1
mkdir objdir && cd objdir
export LD_LIBRARY_PATH=/usr/lib/systemc-3.0.1/lib-linux64/
../configure \
  --with-systemc=/usr/lib/systemc-3.0.1/ \
  --with-json=/home/$USER/Downloads/rapidjson/rapidjson \
  --prefix=/usr/lib/cci-1.0.1
make && sudo make install
```

> **C++20 note:** CCI 1.0.1 requires a patch for C++20 builds (GCC 11+). Apply before
> building: change `typedef void value_type;` to the concrete type in
> `src/cci/core/cci_value.h` at the two locations marked with `// TODO`.
```bash
# C++20 compatibility patch (required for GCC 11+ / -std=c++20)
+# CCI 1.0.1 relies on iterator traits that are stricter in C++20; update the
+# value_type typedefs so the iterator types satisfy std::iterator_traits.
+apply_patch <<'PATCH'
+--- a/src/cci/core/cci_value.h
++++ b/src/cci/core/cci_value.h
+@@ -764,7 +764,7 @@
+   template<typename U> friend class cci_impl::value_iterator_impl;
+   typedef cci_impl::value_ptr<cci_value_map_elem_cref> proxy_ptr;
+ 
+-  typedef void value_type; // TODO: add  explicit value_type 
++  typedef cci_value_map_elem_cref value_type; // TODO: add  explicit value_type 
+ public:
+   typedef cci_value_map_elem_cref const_reference;
+   typedef cci_value_map_elem_ref  reference;
+@@ -791,7 +791,7 @@
+ {
+   template<typename U> friend class cci_impl::value_iterator_impl;
+   typedef cci_impl::value_ptr<cci_value_map_elem_ref> proxy_ptr;
+-  typedef void value_type; // TODO: add  explicit value_type
++  typedef cci_value_map_elem_ref value_type; // TODO: add  explicit value_type
+ public:
+   typedef cci_value_map_elem_cref const_reference;
+   typedef cci_value_map_elem_ref  reference;
+PATCH
+
```

### Configure & Build

**Clone the repository:**

```bash
git clone git@github.com:tenstorrent/tt-oca-sim.git
cd tt-oca-sim
git submodule update --init --recursive
```
> **Note:** Ensure that your GitHub SSH keys are configured correctly, as the CSML submodule uses SSH for cloning.

**Build:**

Export install paths for your machine and C++ standard (see **`vp/vp_build_env.sh`**), then build:

```bash
export SYSTEMC_HOME_C17=/path/to/installs_c17
export CCI_HOME_C17=/path/to/installs_c17
export OPENSSL_ROOT_C17=/path/to/installs_c17/openssl-3.0.13
export BOOST_ROOT_C17=/path/to/installs_c17/boost-1.84.0
export CMAKE_CXX_STANDARD=17

cd vp
source configure_vp.sh
cd build
make sep-vp
```

Override defaults on the command line:

```bash
cd vp
CMAKE_BUILD_TYPE=Release
source configure_vp.sh
cd build
make sep-vp
```

Output binary: `vp/build/bin/sep-vp`

### Runtime Configuration

**`vp/platform/sep/config/accellera_config.ini`** — CCI parameters for all models
(verbosity, algorithm selection, eFuse values, etc.)

**`vp/platform/sep/config/veeriss_config.json`** — VeeR ISS configuration
(XLEN, NMI vector, extensions, ICCM/DCCM layout, CSR overrides).
Referenced from the INI via `och_sep_ss1.configFile`.
vp/platform/sep/config/veeriss_config.json

```bash
{
    "xlen"       : 32,
    "nmi_vec"    : "0x01000e00",

    "iccm": {
        "region": "0xc",
        "offset": "0x00000000",
        "size":   "0x00040000"
    },
    "dccm": {
        "region": "0xc",
        "offset": "0x00040000",
        "size":   "0x00020000"
    },

    "memory_mapped_registers": {
        "address":  "0xc0080000",
        "size":     "0x00006000",
        "internal": "false"
    },

    "enable_zfh" : "true",
    "enable_zba" : "true",
    "enable_zbb" : "true",
    "abi_names"  : "true",
    "enable_A"   : "true",
    "enable_B"   : "true",
    "enable_C"   : "true",
    "enable_D"   : "true",
    "enable_F"   : "true",
    "enable_I"   : "true",
    "enable_M"   : "true",
    "enable_S"   : "true",
    "enable_U"   : "true",
    "enable_V"   : "true",

    "csr" : {
        "mrac" : {
            "number" : "0x7c0",
            "exists" : "true",
            "reset"  : "0x0",
            "mask"   : "0xffffffff",
            "comment": "VeeR EL2 PMA control — stub, writes accepted and ignored in VP"
        }
    }
}
```

### Running the VP

```bash
vp/build/bin/sep-vp vp/platform/sep/config/accellera_config.ini <firmware.elf>
```

### sep-vp-tests (Vayavya peripheral verification)

> Tests under `sw/sep-vp-tests/` were added by Vayavya to verify the correct functional
> behavior of all modeled peripherals end-to-end from firmware on the VeeR EL2 core.

```bash
cd sw/sep-vp-tests/sep-gpio-test
make        # build ELF
make sim    # run on VP
make debug  # run with GDB enabled
make gdb    # connect GDB (second terminal)
```

### Firmware tests from Tenstorrent
Refer to [Building and Running TT Firmware Tests](#building-and-running-tt-firmware-tests).

### Peripheral Model Unit Tests

Each peripheral under `sep/peripherals/<ip>/` follows this layout:

```
<ip>/
├── CMakeLists.txt
├── README.md
├── run_tests.sh
├── include/              ← public headers
├── src/                  ← implementation
├── test/                 ← CTest-registered unit tests
└── docs/
    ├── 01_<IP>_Specification/
    ├── 02_<IP>_HighLevel_Design.md
    └── 03_<IP>_Test_Plan.md
```

Export the same `*_C17` / `*_C20` install paths as for the VP build (see **`vp/vp_build_env.sh`**), then:

```bash
export CMAKE_CXX_STANDARD=17   # or 20 — must match your SystemC/OpenSSL/Boost prefix
source ../../vp/configure_vp.sh   # from repo root: source vp/configure_vp.sh
cd sep/peripherals/<ip>
./run_tests.sh              # build + run
./run_tests.sh --asan       # with AddressSanitizer
./run_tests.sh --coverage   # with lcov coverage report
./run_tests.sh --debug      # debug build
./run_tests.sh --ctest      # ctest
./run_tests.sh --docs       # doxygen docs
```

Or manually:

```bash
CMAKE_CXX_STANDARD=20 # Optional step to override the default setting of C++17
source configure_vp.sh
mkdir build && cd build
cmake .. -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
make && ctest -V
```

`sep/peripherals/run_all_peripherals.sh` builds and runs all peripherals at once.
Logs are written to `sep/peripherals/logs/`.

---

---

## Building and Running TT Firmware Tests

`sw/tt-oca-hw-main/dv/sep/tests/` contains software/firmware DV tests from the TT hardware
repository, verified on the SEP VP.
`sw/tt-oca-hw-main/fw/sep/tests/` contains the main firmware test suite also verified on the SEP VP.

TT tests are managed by `bin/sep_fw_standalone.sh` and are the same ELFs for both flows.

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

> `sw/tt-oca-hw-main/bin/setup_sep_test_env.sh` is TT's internal RHEL/UVM script — not
> for VP firmware testing. Use `sep_fw_standalone.sh` instead.

### One-time setup

```bash
cd sw/tt-oca-hw-main
bin/sep_fw_standalone.sh setup
```

### Build tests (common to both flows)

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
copy and all `dv/sep/tests/` are linked against it via `common.mk`.

### VP firmware test notes

**`sep_outbound_filter_init()`** has been commented out in all firmware tests under
`fw/sep/tests/`. This function initializes the SEP outbound filter which is not modeled
in the VP.

The following IPs are **not modeled** in the VP. Tests that exercise them will fail:

- `PIC` (Platform Interrupt Controller)
- `sep_cpu_ctrl`, `sep_reset_ctrl`
- `local_master_alias_remap_ctrl`
- `och_sep_cdns_spi_ctrl`, `och_sep_spi_mux_ctrl`

### Tests added by Vayavya

The following tests under `sw/tt-oca-hw-main/dv/sep/tests/` were added by Vayavya:

| Test | Purpose |
|---|---|
| `interrupt_test` | Verifies PLIC/CLINT interrupt controller behavior — triggers machine software interrupt (MSIP) via CLINT, timer interrupt (MTIP), and synchronous exception; confirms the VeeR EL2 trap handler dispatches and returns correctly. Check the test code which also references the boot/startup code pattern for this test. |
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
|---|---|
| `otbn_loop` | Loop algorithm |
| `smoke` | Smoke test |
| `p256_ecdsa` | P-256 ECDSA |
| `rsa_3072` | RSA 3072-bit |
| `rsa_2048` | RSA 2048-bit |

### Controlling Verbosity

Each model's verbosity (0–5) via INI

```ini
och_sep_ss1.hmac.verbosity    : 1
och_sep_ss1.otbn.verbosity    : 2
och_sep_ss1.sram.verbosity    : 0
```

### Log Files

| File | Generated by |
|---|---|
| `och_sep_ss.log` | VP run (current directory) |
| `veer_trace.log` | VeeR ISS instruction trace |
| `veer_inst_freq.log` | VeeR ISS instruction frequency |

Log file names can be changed via CCI parameters.


### Compiler Selection

Use a GCC version that supports the C++ standard selected via `CMAKE_CXX_STANDARD` in `vp/configure_vp.sh`.

On **RHEL 8**, GCC Toolsets can be used to select a newer compiler. For example, to build with GCC 11:

```bash
# Open a shell with GCC 11 on PATH
scl enable gcc-toolset-11 bash

cd vp
export CMAKE_CXX_STANDARD=20
source configure_vp.sh
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
