# Tenstorrent SEP

## Overview

This project has SystemC models and the Virtual Platform (VP) for Tenstorrent OCH SEP (Secure Enclave Processor).

## Dependencies

### For Building VP

*   **SystemC**: Version 3.0.1 or higher.
*   **CMake**: Version 3.24 or higher.
*   **Boost Libraries**: `iostreams`, `program_options`, and `log` components are required.
*   **C++ Compiler**: Must support C++17 (e.g., GCC 9+).
*   **CCI**: Accellera CCI library for configuration or the Synopsys SCML library

### For Building Software

*   **RISC-V Toolchain**: A GCC toolchain targeting RISC-V 32-bit ELF is required (e.g., `riscv32-unknown-elf-gcc`).
In order to test the software examples, a configured RISC-V GNU toolchain is required in your $PATH.

For more information on prerequisites and installing  RISC-V GNU toolchain visit https://github.com/riscv-collab/riscv-gnu-toolchain
*   **Make**: GNU Make is required to run the build scripts.

### Simulation time tools

*   **gdb-multiarch**: Debugger for RISC-V software. Required for debugging applications running on the VP.
*   **telnet**: Optional, for connecting to the UART terminal.

### Other tools

*   **gcov/lcov**: For code coverage analysis.
*   **Doxygen**: For generating API documentation.
*   **Graphviz**: (Optional) For generating call graphs in Doxygen documentation.

## Detailed installation steps

### 1. System Update and Basic Tools

```bash
# Update package lists
sudo apt-get update

# Install essential build tools
sudo apt install -y g++ make cmake
```

### 2. Build and Install SystemC 3.0.1

SystemC is a C++ library for system-level modeling and simulation.

```bash
# Create working directory
mkdir -p ~/Downloads
cd ~/Downloads

# Download SystemC 3.0.1
wget https://github.com/accellera-official/systemc/archive/refs/tags/3.0.1.tar.gz

# Extract archive
tar zxvf 3.0.1.tar.gz
cd systemc-3.0.1

# Create build directory (out-of-source build)
mkdir objdir
cd objdir

# Create installation directory (modify as required)
sudo mkdir -p /usr/lib/systemc-3.0.1


# Configure 
../configure --prefix=/usr/lib/systemc-3.0.1

# Workaround: Create empty file if missing (known bug in SystemC 3.0.1)
touch ../docs/DEVELOPMENT.md

# Build SystemC
make

# Install to system location
sudo make install
```

### 3. Setup RapidJSON

RapidJSON is a JSON parser/generator library required by CCI.

```bash
# Change directory to clone rapidjson as needed
cd ~/Downloads
mkdir -p rapidjson
cd rapidjson

# Clone RapidJSON repository
git clone https://github.com/Tencent/rapidjson.git
```

**Note:** RapidJSON is header-only, so no build/install is needed.

---

### 4. Build and Install CCI 1.0.1

CCI (Configuration, Control and Inspection) is a SystemC extension for configuration management.

```bash
cd ~/Downloads

# Download CCI 1.0.1
wget https://github.com/accellera-official/cci/releases/download/v1.0.1/cci_v1.0.1.tar.gz

# Extract archive
tar zxvf cci_v1.0.1.tar.gz
cd cci_v1.0.1

# Install autoconf (required for CCI build system)
sudo apt install -y autoconf

# Create installation directory
sudo mkdir -p /usr/lib/cci-1.0.1

# Create build directory
mkdir objdir
cd objdir

# Set library path for SystemC
# The CCI configure script needs to run test programs that link against SystemC
export LD_LIBRARY_PATH=/usr/lib/systemc-3.0.1/lib-linux64/

# Configure CCI - modify paths below as required
../configure \
  --with-systemc=/usr/lib/systemc-3.0.1/ \
  --with-json=/home/ubuntu/Downloads/rapidjson/rapidjson \
  --prefix=/usr/lib/cci-1.0.1

# Build CCI
make

# Install CCI
sudo make install
```

**Important Notes:**
- The `LD_LIBRARY_PATH` must be set before running configure
- Without it, the configure script fails with "Non IEEE 1666 compatible SystemC version found"

---

### 5. Install Additional Dependencies

```bash
# Boost libraries (required for VP)
sudo apt-get install -y \
  libboost-iostreams-dev \
  libboost-program-options-dev \
  libboost-log-dev

# Documentation generation tools
sudo apt install -y doxygen graphviz

# OpenSSL development libraries (required for HMAC and other crypto models)
sudo apt install -y libssl-dev

# VNC server development libraries (required for display support)
sudo apt install -y libvncserver-dev
```

## Directory structure

```
.
|-- models                 -> Individual peripheral models
|   |-- generic            -> Generic models (UART, Memory) 
|   |-- ot                 -> OpenTitan based models (HMAC, OTBN, etc)
|   |-- utils              -> Utility libraries
|   `-- veer-iss           -> Veer/El2 model. 
|       |-- VeeR-ISS       -> Includes VeeR/El2 core files from `https://github.com/chipsalliance/VeeR-ISSA`
|       `-- VeeR-ISSTlm    -> SystemC Tlm VeeRISSTlm wrapper.
|-- riscv-vp-plusplus      -> RISC-V core model and core sub-system models
|   |-- sw                 -> Test software, including DV tests
|   `-- vp                 -> VP top-levels, including OCH SEP top
```

Sub-system models (NoC, PLIC, etc) are from https://github.com/agra-uni-bremen/riscv-vp

## Build the VP

The VP supports two build flows:

1. Accellera SystemC flow: Builds the `sep-vp` executable.
2. Synopsys Virtualizer flow: Builds the merged static model library (`libOCHSEPModels.a`) which is then used by the Virtualizer TLM block `OCH_SEP_SS_SNPS`.

### Accellera SystemC flow

```bash
cd riscv-vp-plusplus/vp
mkdir build
cd build

# Configure with CMake
cmake .. -DVP_SYSC_BACKEND=accellera

# Build the VP
make sep-vp
```

This produces the `sep-vp` executable under:

```
riscv-vp-plusplus/vp/build/bin/sep-vp
```

### Synopsys Virtualizer flow

Edit the `set_snps_third_party_env.sh` file to update the paths to the third party tools and libraries as per your installation.

```bash
source riscv-vp-plusplus/vp/src/platform/sep/set_snps_third_party_env.sh
```

```bash
cd riscv-vp-plusplus/vp
mkdir build_synopsys
cd build_synopsys

cmake .. -DVP_SYSC_BACKEND=synopsys  
```

CMake produces only:

- `libOCHSEPModels.a` (merged static archive of model libraries)
- the library is created under `riscv-vp-plusplus/vp/src/platform/sep`.

Note: In the Synopsys flow, CMake does not build a standalone `sep-vp` executable.

Note: In the Synopsys flow, CMake is used to build the merged model library. The `OCH_SEP_SS_SNPS` shared library is built in Synopsys Virtualizer (via the TLM Creator project), not as a standalone `sep-vp` executable.

In the Virtualizer GUI, open the TLM project from filesystem at `riscv-vp-plusplus/vp/src/platform/sep/OCH_SEP_SS_SNPS`

This project picks up `OCH_SEP_SS_SNPS.tlmc` and links against the library produced by the Synopsys flow. Build the project to create the `OCH_SEP_SS_SNPS` building block, which can be further used in a VDK.

### Build types

By default, the build type is `Debug`. To build for release (optimized):

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release -DVP_SYSC_BACKEND=accellera
cmake .. -DCMAKE_BUILD_TYPE=Release -DVP_SYSC_BACKEND=synopsys

```

## Build the Software

The software sources are located in `riscv-vp-plusplus/sw`.

To build the SEP specific tests (e.g., for GPIO):

```bash
cd riscv-vp-plusplus/sw/sep-vp-tests/sep-gpio-test
make
```

This will create the ELF binary (e.g., `sep_gpio_test`) which can be loaded into the VP.

### Debugging Software with GDB

The SEP VP supports debugging RISC-V software using GDB. For detailed debugging instructions, see:
**[riscv-vp-plusplus/sw/sep-vp-tests/README.md](riscv-vp-plusplus/sw/sep-vp-tests/README.md)**

Quick start:
```bash
# Terminal 1: Start VP in debug mode
make debug

# Terminal 2: Connect GDB
make gdb
```

## Running the simulation

 To run the simulation, you can use the `make sim` target from a test directory under `riscv-vp-plusplus/sw/sep-vp-tests/*`.

 `make sim` runs the Accellera `sep-vp` executable with a default configuration `.ini` that sets the runtime parameters for the OCH SEP Sub System(SS) models:

 - **INI file path (used by default by `make sim`)**

 ```
 riscv-vp-plusplus/vp/src/platform/sep/accellera_config.ini
 ```

 The `.ini` file also selects the VeeR ISS JSON configuration via the `och_sep_ss1.configFile` setting (see the `.ini`):

 - **JSON file path (referenced from the `.ini`)**

 ```
 riscv-vp-plusplus/vp/src/platform/sep/veeriss_config.json
 ```

 The default contents of `veeriss_config.json` are:

 ```
 {
     "xlen"       : 32,
     "nmi_vec"    : "0x01000e00",
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

 The `nmi_vec` setting specifies the address used as the NMI handler entry point by the VeeR ISS. By default, this value is taken from `veeriss_config.json`. Some firmware tests override the NMI vector at runtime by writing a mailbox register with the desired vector (this write is also printed), and the VP can intercept these mailbox/stdout writes based on a specific write pattern to apply the updated NMI vector. This mechanism exists because the final customer requirement for NMI vector programming is currently unclear.

 For all other runtime parameter values used by the VP (and the possible values supported), refer to the `.ini` file:
 `riscv-vp-plusplus/vp/src/platform/sep/accellera_config.ini`.

 Run the simulation:

 ```bash
 make sim
 ```

 Alternatively, executing the VP binary with only the `accellera_config.ini` file; the ELF target will be picked from the `.ini` 

 ```bash
 $ tenstorrent_sep/riscv-vp-plusplus/vp/build/bin/sep-vp tenstorrent_sep/riscv-vp-plusplus/vp/src/platform/sep/accellera_config.ini 
```

 Alternatively, executing the VP binary directly with the compiled software ELF as an argument:

 ```bash
# Assuming you are in the software test directory (e.g., riscv-vp-plusplus/sw/sep-vp-tests/sep-gpio-test)
 $ ../../../../vp/build/bin/sep-vp ../../../../vp/src/platform/sep/accellera_config.ini sep_gpio_test
```

 For the `riscv-vp-plusplus/sw/sep-vp-tests/uart_16550_test` test, it is recommended to run the VP in a `tmux` session for better UART output visualization:

 ```bash
 tenstorrent_sep/riscv-vp-plusplus/vp/build/bin$ tmux
 tenstorrent_sep/riscv-vp-plusplus/vp/build/bin$ ./sep-vp ../../src/platform/sep/accellera_config.ini ../../../sw/sep-vp-tests/uart_16550_test/uart_16550_test
 ```

 You can add the VP build directory to your `PATH` (e.g., in `~/.bashrc`) to run `sep-vp` from anywhere:

```bash
export PATH=$PATH:/path/to/tenstorrent_sep/riscv-vp-plusplus/vp/build/bin
```

## Compiling and simulating TT tests

Compiling TT tests require few more tools to be installed. The TT software is
present in `riscv-vp-plusplus/sw/tt-oca-hw-main/`. Please refer
to the `README.partners.md` file in that folder for setup and compliation
instructions.

Once the software is compiled, they can be run from
`riscv-vp-plusplus/sw/tt-tests/` folder - refer to the `README.md` file in that
folder for more information.

### Running OTBN tests

The algorithm that will execute on the OTBN SystemC model can be configured. 
To set an algorithm type for Accellear flow, you need to change the alogirthm type parameter in the config.ini file. For the synopsys flow, set the algorithm type parameter in the Virtualizer GUI to the desired algorithm value.

   Following are the availabe algorithms and the parameter value to be used:
rsa 2048 key enabled algorithm : rsa_2048
summation alogirthm : summation
loop algorithm : otbn_loop
random test algorithm : rnd_test
p256 ecdsa algorithm : p256_ecdsa

### Controlling Debug Verbosity
Verbosity of each model can be configured from 0 to 5. For Accellera flow, the verbosity needs to be edited in the `accellera_config.ini`. For Synopsys Virtualizer flow, the verbosity needs to be edited in the GUI.

When running the Accellera `sep-vp` executable, CSML logs are printed to stdout and also appended to a log file created in the current working directory:

```
och_sep_ss.log
```
