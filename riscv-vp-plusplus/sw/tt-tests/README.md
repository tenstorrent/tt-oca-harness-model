# TT Tests - Quick Reference Guide

Tenstorrent firmware tests for SEP Virtual Platform. These tests are built from `tt-oca-hw-main/` and run on the VP.

## Prerequisites

### System Packages
```bash
sudo apt-get update
sudo apt-get install -y python3 python3-venv perl libbit-vector-perl
sudo apt-get install meson
```

### Rust/Cargo (for Bender dependency manager)
```bash
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
source $HOME/.cargo/env
```

### RISC-V Toolchain
Ensure `riscv32-unknown-elf-gcc` is in your PATH.

### SEP VP
VP must be built at `../../vp/build/bin/sep-vp`

---

## One-Time Setup

### Step 1: Fix Bender.yml Dependencies

```bash
cd ../tt-oca-hw-main
nano Bender.yml
```
**Ensure lines 22-46** (TT internal GitLab repos) are commented out. They should already be commented.

Save and exit.

### Step 2: Run Setup

```bash
./bin/sep_fw_standalone.sh setup
```

This will:
- Create Python virtual environment
- Generate register files
- Checkout VeeR EL2 RISC-V core
- Configure VeeR EL2 for SEP
- Build picolibc C library

**Takes 5-10 minutes.** Only needs to be run once.

---

## Building Tests

### Build a Specific Test
```bash
cd riscv-vp-plusplus/sw/tt-oca-hw-main
./bin/sep_fw_standalone.sh build <test_name>
```

### Build All Tests
```bash
./bin/sep_fw_standalone.sh build-all
```

### Available Tests
- `hello_world` - Basic test
- `hmac_test` - HMAC cryptographic hash
- `aes_test` - AES encryption
- `kmac_test` - Keyed MAC
- `otbn_loops_test` - OTBN big number accelerator loops
- `otbn_p256_verify_test` - OTBN P256 ECDSA verification
- `uart` - UART communication
- `dma_test` - DMA controller
- `memory_sanity` - Memory test
- `rom_sanity_test` - ROM test

### Build Artifacts
Located in `dv/sep/tests/<test_name>/`:
- `<test_name>.elf` - Executable binary
- `<test_name>.dis` - Disassembly
- `<test_name>.itcm.hex` - Instruction memory
- `<test_name>.dtcm.hex` - Data memory

---

## Running Tests on VP

### Basic Execution

After building a test, navigate to the corresponding `tt-tests/` directory:

```bash
cd ../../tt-tests/sep-hmac-tt-test
make sim
```

### Test Directory Mapping

| Test Name | tt-tests Directory |
|-----------|-------------------|
| `hmac_test` | `sep-hmac-tt-test/` |
| `otbn_loops_test` | `sep-otbn-loops-tt-test/` |
| `otbn_p256_verify_test` | `sep-otbn-p256-tt-test/` |
| `uart` | `sep-uart-tt-test/` |

**Note:** The Makefiles in `tt-tests/` do NOT build the ELF - they only run the pre-built ELF from `tt-oca-hw-main/dv/sep/tests/`. Always build first, then run.

---

## Debugging with GDB

### Start Debug Session

**Terminal 1 - Start VP in debug mode:**
```bash
cd tt-tests/sep-hmac-tt-test
make debug
```
VP starts as GDB server on port 5005.

**Terminal 2 - Connect GDB client:**
```bash
cd tt-tests/sep-hmac-tt-test
make gdb
```

### GDB Commands
```gdb
(gdb) break main              # Set breakpoint
(gdb) continue                # Run to breakpoint
(gdb) step                    # Step into
(gdb) next                    # Step over
(gdb) print variable          # Inspect variable
(gdb) info registers          # View RISC-V registers
(gdb) x/10i $pc               # Disassemble instructions
(gdb) quit                    # Exit (terminates VP)
```

### Custom Debug Port
```bash
# Terminal 1
DEBUG_PORT=5006 make debug

# Terminal 2
DEBUG_PORT=5006 make gdb
```

### Generate Disassembly
```bash
make dump
# Creates .dump and .sym files for reference
```

---

## Complete Workflow Example

**First time setup:**
```bash
# Install prerequisites
sudo apt-get install -y python3 python3-venv perl libbit-vector-perl
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
source $HOME/.cargo/env

# Fix Bender.yml and run setup
cd riscv-vp-plusplus/sw/tt-oca-hw-main
nano Bender.yml  # Change line 49: git@github.com → https://github.com
./bin/sep_fw_standalone.sh setup
```

**Build and run a test:**
```bash
# Build (from tt-oca directory)
./bin/sep_fw_standalone.sh build hmac_test

# Run (from tt-tests directory)
cd ../../tt-tests/sep-hmac-tt-test
make sim
```

**Build and run all tests:**
```bash
# Build all (from tt-oca directory)
cd riscv-vp-plusplus/sw/tt-oca-hw-main
./bin/sep_fw_standalone.sh build-all

# Run each test
cd ../../tt-tests/sep-hmac-tt-test && make sim
cd ../sep-otbn-loops-tt-test && make sim
cd ../sep-otbn-p256-tt-test && make sim
cd ../sep-uart-tt-test && make sim
```

---

## Troubleshooting

### Setup Fails: "cargo not found"
Install Rust:
```bash
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
source $HOME/.cargo/env
```

### Setup Fails: "Can't locate Bit/Vector.pm"
Install Perl module:
```bash
sudo apt-get install -y libbit-vector-perl
```

### Setup Fails: "Permission denied (publickey)" for el2
Fix Bender.yml line 49 - change from `git@github.com` to `https://github.com`

### Build Fails: "deps/el2/tools/picolibc.mk not found"
Setup didn't complete. Re-run:
```bash
rm -rf .bender deps/el2
./bin/sep_fw_standalone.sh setup
```

### "make sim" Fails: "No such file or directory" for .elf
Build the test first:
```bash
cd ../../tt-oca-hw-main
./bin/sep_fw_standalone.sh build <test_name>
```

### VP Not Found
Ensure SEP VP is built:
```bash
cd ../../../vp/build/bin
ls -la sep-vp  # Should exist
```

### Clean and Rebuild
```bash
# Clean specific test
./bin/sep_fw_standalone.sh clean <test_name>

# Clean everything
./bin/sep_fw_standalone.sh clean-all
```

---

## Key Differences from sep-vp-tests

| Aspect | sep-vp-tests | tt-tests |
|--------|--------------|----------|
| **Source** | Simple standalone tests | Tenstorrent official firmware |
| **Build System** | Simple Makefile | Complex (Bender, Meson, picolibc) |
| **Setup** | None required | One-time setup required |
| **Build Location** | Test directory | Separate tt-oca directory |
| **Run Location** | Same directory | tt-tests wrapper directory |
| **Dependencies** | Minimal | Rust, Python, Perl modules, VeeR EL2 |

---

## VP Integration Notes

The following changes were made to SEP VP to support TT tests:

1. Added ITCM and DTCM support (per TT linker script)
2. Added `stdout_device` stub for picolibc printf
3. HMAC address changed from `0x40026000` to `0x40088000`
4. Commented `csrw 0x7c0, t0` in `crt0.s`
5. Added `'-march=rv32i_zicsr'` in `otbn_as.py` for P256 test

These changes are already present in the codebase.

---

## Additional Resources

- **TT Build Documentation**: `tt-oca-hw-main/README.partners.md`
- **SEP VP Tests**: `../sep-vp-tests/README.md`
- **Main Project README**: `../../../../README.md`
- **RISC-V VP Documentation**: `../../../vp/README.md`
