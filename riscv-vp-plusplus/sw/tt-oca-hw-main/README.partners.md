# SEP Firmware Standalone Build Guide

This guide provides instructions for building SEP (Security Processor) firmware using the standalone build script without requiring the full OCH development environment.

## Prerequisites

### RISC-V Toolchain

A precompiled RISC-V 64-bit GNU toolchain for Linux is provided as a zip archive. You need to extract it before using the build script.

**Step 1: Extract the Toolchain**

The toolchain zip file is located in the repository root:
- `riscv-gnu-toolchain-2025.01.20-rhel-8.10.zip` (approximately 3.2GB)

Extract it to the `toolchain/` directory:

```bash
cd toolchain/
unzip ../riscv-gnu-toolchain-2025.01.20-rhel-8.10.zip
cd ..
```

The extracted toolchain will be located at:
- `toolchain/tools_soc/opensrc/riscv-gnu-toolchain/2025.01.20-rhel-8.10/`

The build script will automatically detect and use this toolchain. No additional configuration is needed.

**Alternative: Using Your Own Toolchain**

If the provided toolchain doesn't work on your system, you can:
1. Download and install the toolchain from [RISC-V GNU Toolchain](https://github.com/riscv-collab/riscv-gnu-toolchain/releases)
2. Set the toolchain path before running the script:
   ```bash
   export RISCV_TOOLCHAIN_PATH=/path/to/your/riscv-toolchain
   ```

### System Requirements

The build script automatically handles:
- **Bender** (dependency manager) - installs via cargo if not found
- **Python virtual environment** - creates and configures automatically
- **Meson build system** - installs in the virtual environment

You only need to have these base tools installed:
- Python 3
- Rust/cargo (for Bender installation, if not already installed)
- Perl (for VeeR EL2 configuration)

## Quick Start

### Step-by-Step Instructions

**Step 1: Extract the Toolchain** (if not already done)

```bash
cd toolchain/
unzip ../riscv-gnu-toolchain-2025.01.20-rhel-8.10.zip
cd ..
```

**Step 2: Run One-Time Setup**

This step handles all dependencies, register file generation, VeeR EL2 core configuration, and picolibc C library build. This only needs to be run once:

```bash
./bin/sep_fw_standalone.sh setup
```

The setup process will:
1. Create and configure a Python virtual environment
2. Generate register files (required for Bender dependency checkout)
3. Checkout dependencies using Bender (including VeeR EL2 RISC-V core)
4. Configure the VeeR EL2 core for SEP
5. Build picolibc (RISC-V C library)

**Note:** If you run setup again, it will automatically skip steps that are already completed.

**Step 3: Build a Test**

After setup is complete, you can build any test:

```bash
./bin/sep_fw_standalone.sh build hello_world
```

The build artifacts will be generated in `dv/sep/tests/hello_world/`:
- `hello_world.elf` - Executable binary
- `hello_world.itcm.hex` - Instruction memory hex file
- `hello_world.dtcm.hex` - Data memory hex file
- `hello_world.dis` - Disassembly listing

## Available Commands

The standalone script provides these commands:

```bash
# One-time setup (Python venv, VeeR EL2 config, picolibc build)
./bin/sep_fw_standalone.sh setup

# Build a specific test
./bin/sep_fw_standalone.sh build <test_name>

# Build all tests
./bin/sep_fw_standalone.sh build-all

# Clean specific test
./bin/sep_fw_standalone.sh clean <test_name>

# Clean everything
./bin/sep_fw_standalone.sh clean-all
```

Available tests: `hello_world`, `aes_test`, `dma_test`, `hmac_test`, `kmac_test`, `memory_sanity`, `otbn_loops_test`, `otbn_p256_verify_test`, `rom_sanity_test`, `uart`

## Getting Help

View all available commands:

```bash
./bin/sep_fw_standalone.sh help
```

## Build Artifacts

After building a test, the following files are generated in `dv/sep/tests/<test_name>/`:
- `<test_name>.elf` - Executable binary
- `<test_name>.itcm.hex` - Instruction memory hex file
- `<test_name>.dtcm.hex` - Data memory hex file
- `<test_name>.dis` - Disassembly listing

## Troubleshooting

**Toolchain not found**:
- Ensure you have extracted the toolchain zip file to `toolchain/` directory
- The script will automatically detect the toolchain at `toolchain/tools_soc/opensrc/riscv-gnu-toolchain/2025.01.20-rhel-8.10/`
- If using your own toolchain, set `RISCV_TOOLCHAIN_PATH` environment variable:
  ```bash
  export RISCV_TOOLCHAIN_PATH=/path/to/your/riscv-toolchain
  ```
- Or add `riscv64-unknown-elf-gcc` to your PATH manually

**Cargo/Bender not found**:
- Install Rust from https://rustup.rs/
- The script will automatically install Bender when needed
- Ensure `~/.cargo/bin` is in your PATH

**Build failures**:
- Ensure base tools are installed (Python 3, Perl)
- If setup fails partway through, you can run individual steps:
  ```bash
  ./bin/sep_fw_standalone.sh venv      # Setup Python venv only
  ./bin/sep_fw_standalone.sh config    # Configure EL2 only
  ./bin/sep_fw_standalone.sh picolibc  # Build picolibc only
  ```
- To start fresh, run `./bin/sep_fw_standalone.sh clean-all` and try setup again

**Register generation errors**:
- If register generation fails, ensure Python virtual environment is activated
- The script will automatically generate registers during setup, but you can regenerate manually if needed
