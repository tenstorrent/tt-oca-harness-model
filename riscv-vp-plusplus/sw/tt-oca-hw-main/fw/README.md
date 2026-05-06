# FW Sep Tests — Build and Run Guide

Firmware tests under `fw/sep/tests/` target the VeeR EL2 core running on the SEP subsystem.
They can be built for two platforms:

| `PLATFORM` | Target | ITCM address | DTCM address |
|---|---|---|---|
| `vp` | Virtual Platform (`sep-vp`) | `0x01000000` | `0x00000000` |

---

## 1. Prerequisites

### One-time: build the VP binary

```bash
cd riscv-vp-plusplus/vp

mkdir build/

cd build/

cmake ..

make

# output: vp/build/bin/sep-vp
```

### One-time: build picolibc (C runtime)

```bash
cd riscv-vp-plusplus/sw/tt-oca-hw-main
./bin/sep_fw_standalone.sh setup
```

---

## 2. Environment Setup

Source this script once per terminal session before building or running any `fw/sep` test:

```bash
# From the project root (riscv-vp-plusplus/sw/tt-oca-hw-main/):
source fw/setup_fw_env.sh

# Or from inside the fw/ directory:
source setup_fw_env.sh
```

This sets `OCH_ROOT` and `RV_ROOT` to the correct paths relative to this repository.

---

## 3. Build a Test

```bash
cd fw/sep/tests/<test_name>

# VP build — for use with the Virtual Platform
make

```

Both produce `<test_name>.elf` in the test directory.

### Example

```bash
cd fw/sep/tests/spi_ot_flash_write_read_test
make
# produces: spi_ot_flash_write_read_test.elf
```

To rebuild cleanly:

```bash
make clean
```

---

## 4. Run on the Virtual Platform

From the VP binary directory:

```bash
cd riscv-vp-plusplus/vp/build/bin

./sep-vp \
    ../../src/platform/sep/accellera_config.ini \
    ../../../sw/tt-oca-hw-main/fw/sep/tests/<test_name>/<test_name>.elf
```

### Example — SPI flash write/read test

```bash
cd riscv-vp-plusplus/vp/build/bin

./sep-vp \
    ../../src/platform/sep/accellera_config.ini \
    ../../../sw/tt-oca-hw-main/fw/sep/tests/spi_ot_flash_write_read_test/spi_ot_flash_write_read_test.elf
```

Simulation output is printed to the terminal and also written to `och_sep_ss.log` in the
directory where `sep-vp` is invoked.

### Alternatively: override the target via the ini file

Set `och_sep_ss1.targets` in `accellera_config.ini` to point at the ELF, then run without
the path argument:

```ini
[string]
och_sep_ss1.targets : ../../../sw/tt-oca-hw-main/fw/sep/tests/spi_ot_flash_write_read_test/spi_ot_flash_write_read_test.elf
```

```bash
cd riscv-vp-plusplus/vp/build/bin
./sep-vp ../../src/platform/sep/accellera_config.ini
```
