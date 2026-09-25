# SEP VP Software Tests

This directory contains bare-metal software tests for the SEP Virtual Platform peripherals.

## Run all tests automatically

`run_sep_vp_tests.sh` is a host-agnostic runner (macOS / RHEL / Ubuntu) that
auto-detects the RISC-V toolchain, the `sep-vp` executable, and the required
SystemC/CCI/Whisper/Boost install locations, then builds and runs the firmware
tests. If `sep-vp` is missing, the script builds it for you.

```bash
cd sw/sep-vp-tests

./run_sep_vp_tests.sh --list          # list available tests
./run_sep_vp_tests.sh                 # build + run all tests
./run_sep_vp_tests.sh sep-crng-test   # build + run a single test by name
./run_sep_vp_tests.sh -i              # choose a single test from a numbered menu
./run_sep_vp_tests.sh --build-vp      # (re)build sep-vp first, then run all
```

The script searches common install prefixes and `PATH` for the toolchain. If your
setup is non-standard, override detection with environment variables:

```bash
RISCV_PREFIX=riscv64-elf- \
SYSTEMC_HOME=/path/to/systemc-3.0.2-cxx20 \
CCI_HOME=/path/to/cci-cxx20 \
WHISPER_HOME=/path/to/whisper \
BOOST_DIR=/opt/homebrew/opt/boost \
./run_sep_vp_tests.sh
```

> **Note:** The SEP VP does not self-terminate when firmware finishes; it keeps
> running until you press Ctrl+C. The runner simply invokes `make sim` for each
> test, matching the manual workflow below. Use it for one test at a time, or stop
> each run with Ctrl+C when the firmware output is complete.

## Available Tests

| Directory | Description |
|-----------|-------------|
| `sep-hmac-test/` | HMAC peripheral register access test |
| `hmac-dv-test/` | HMAC design-verification test |
| `opentitan-hmac-test/` | OpenTitan-compatible HMAC DIF test (SHA-256 + HMAC-SHA-256) |
| `sep-crng-test/` | CSRNG peripheral test |
| `sep-edn-test/` | EDN peripheral test |
| `sep-entropy-pool-test/` | Entropy pool STATUS/DATA + PIC 37–39 via EDN endpoint 2 |
| `sep-aon-timer-test/` | AON Timer peripheral test |
| `sep-efuse-test/` | eFuse / OTP peripheral test |
| `sep-mailbox-test/` | Mailbox peripheral test |
| `sep-kmac-dv-test/` | KMAC design-verification test |
| `sep-spi-test/` | SPI controller test |
| `otbn-dv-test/` | OTBN (OpenTitan Big Number) design-verification test |
| `otbn-dv-p256-verify-test/` | OTBN P-256 ECDSA verification test |
| `rom_test/` | ROM read test |
| `fw-tests-from-tt-oca-hw/fw/sep/tests/sep_abr_*` | Adams Bridge (`abr_ip` @ `0x1094_0000`): CSR identity, software-seed keygen/sign, KAT, NIST ACVP, KM seed sideload. Run via `fw-tests-from-tt-oca-hw/fw/sep/tests/run_all_tests.sh` or `run_test.sh sep_abr_csr_test`. |

### Shared Support Code (`common/`)

All tests share the following files from `common/`:

| File | Purpose |
|------|---------|
| `start.S` | Minimal boot code: sets up stack pointer and jumps to `main` |
| `link.ld` | Linker script: memory regions (ROM, SRAM, peripherals) |
| `printf.c` | Bare-metal `printf` (supports `%s`, `%u`, `%x`, `%0Nx`) via stdout device at `0x80000000` |
| `och_sep_common.h` | Common register helpers and peripheral base addresses |

### OpenTitan Compatibility Layer (`opentitan-compat/`)

Provides a subset of the OpenTitan DIF (Device Interface Function) API for use in OpenTitan-style tests:

| File | Purpose |
|------|---------|
| `dif_hmac_sep.c/.h` | OpenTitan HMAC DIF adapted for the SEP VP |
---

## Building and Running Tests

The quickest way to run tests is the `run_sep_vp_tests.sh` helper
(see [Run all tests automatically](#run-all-tests-automatically) above). The
sections below describe the manual `make` workflow for individual tests and
debugging.

### Prerequisites

- **RISC-V GNU toolchain** in your `$PATH` — `riscv64-unknown-elf-gcc` on
  Ubuntu/RHEL, `riscv64-elf-gcc` from Homebrew on macOS. The Makefiles detect
  either one.
- **SEP VP** built at `../../vp/build/bin/sep-vp`
  (from repo root: `cd vp/build && make`)

The runner will build `sep-vp` automatically if it is not found.

### Build a Test

Navigate to any test directory and run `make`:

```bash
cd sep-crng-test
make
```

This produces an ELF binary (e.g., `sep_crng_test`) linked against `rv32imc` / `ilp32`.

### Run the Test

```bash
make sim
```

Launches the SEP VP with `accellera_config.ini` and the test ELF. The VP runs until
the firmware completes or you press **Ctrl+C** (the VP does not self-terminate when
firmware finishes — this is by design).

### Generate Disassembly

```bash
make dump
```

Creates `<target>.dump` (full disassembly) and `<target>.sym` (symbol table).

### Clean Build Artifacts

```bash
make clean
```
---

## Debugging Software with GDB

### Prerequisites

**Ubuntu:**

```bash
sudo apt-get install gdb-multiarch
```

**Note:** The remote debugger binary name varies by platform:

- **Ubuntu:** `gdb-multiarch`
- **RHEL:** May be provided by a different package or toolchain.
- **macOS:** Typically the GDB binary installed with the RISC-V toolchain or via Homebrew.

Create a symbolic link named `gdb-multiarch` that points to the appropriate RISC-V GDB executable on your system. For example:

```bash
ln -s /path/to/riscv-gdb /path/to/gdb-multiarch
```

### Enabling GDB in the VP

GDB support is **disabled by default**. Enable it in the INI before running `make debug`:

```ini
# vp/platform/sep/config/accellera_config.ini
och_sep_ss1.gdb        : true
och_sep_ss1.gdbTcpPort : [4000]
```

The GDB port is read automatically from the INI by `Makefile.common`.

### Debug Workflow

**Terminal 1 — Start VP as GDB server:**

```bash
cd sep-crng-test
make debug
```

Expected output:
```
Starting sep-vp in debug mode on port 4000...
Connect with GDB using: make gdb
```

The VP waits for a GDB connection before executing code.

**Terminal 2 — Connect GDB client:**

```bash
cd sep-crng-test
make gdb
```

Launches `gdb-multiarch`, connects to `localhost:4000`, and sets the architecture to `riscv:rv32`.

### Useful GDB Commands

```gdb
(gdb) break main              # Breakpoint at main()
(gdb) continue                # Run until breakpoint
(gdb) step                    # Step into (source level)
(gdb) stepi                   # Step one instruction
(gdb) next                    # Step over
(gdb) nexti                   # Next instruction
(gdb) print variable_name     # Inspect variable
(gdb) print/x $pc             # Program counter (hex)
(gdb) info registers          # All RISC-V registers
(gdb) x/10i $pc               # Disassemble 10 instructions at PC
(gdb) x/4xw 0x10911000        # Examine 4 words at address (hex)
(gdb) backtrace               # Call stack
(gdb) quit                    # Exit GDB (also terminates VP)
```

### Examining Peripheral Registers

```gdb
# Read HMAC STATUS register
(gdb) x/1xw 0x10911018

# Watch for writes to HMAC CMD register
(gdb) watch *0x10911014
```

### RISC-V Register Conventions

| Register | ABI Name | Role |
|----------|----------|------|
| x1  | `ra` | Return address |
| x2  | `sp` | Stack pointer |
| x10–x17 | `a0–a7` | Function arguments |
| x10–x11 | `a0–a1` | Function return values |

### Troubleshooting GDB

| Symptom | Fix |
|---------|-----|
| `make debug` exits immediately with an error | Set `och_sep_ss1.gdb : true` in the INI |
| `make gdb` — "Connection refused" | Ensure `make debug` is running in Terminal 1 |
| GDB connects but code doesn't match | `make clean && make` to rebuild with debug symbols |
| VP hangs after GDB connects | Set a breakpoint before `continue` |

---

## Test Structure

Each test follows this layout:

```
test-name/
├── Makefile      # Defines SRCS, OBJS, TARGET; includes ../Makefile.common
└── main.c        # Test logic
```

Most tests reference shared files from `../common/`:

```makefile
# Typical Makefile
SRCS   = ../common/start.S main.c ../common/printf.c
OBJS   = $(SRCS:.c=.o)
OBJS  := $(OBJS:.S=.o)
TARGET = my_test_name
LDFLAGS += -T ../common/link.ld

include ../Makefile.common
```

Tests that use the OpenTitan DIF layer also link `opentitan-compat/`:

```makefile
SRCS = ../common/start.S main.c \
       ../opentitan-compat/dif_hmac_sep.c \
       ../opentitan-compat/uart_output.c
```

---

## Makefile Targets Reference

| Target | Description |
|--------|-------------|
| `make` / `make all` | Build the test ELF |
| `make sim` | Run test on SEP VP |
| `make debug` | Start VP as GDB server (GDB must be enabled in INI) |
| `make gdb` | Connect `gdb-multiarch` to running VP |
| `make dump` | Generate `.dump` (disassembly) and `.sym` (symbol table) |
| `make clean` | Remove build artifacts |

---

## Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `RISCV_PREFIX` | first of `riscv64-unknown-elf-`, `riscv64-elf-`, `riscv-none-elf-`, `riscv64-linux-gnu-`, `riscv-none-embed-` found in `$PATH` | Toolchain prefix |
| `RISCV_TOOLCHAIN_PATH` | _(unset)_ | Base dir holding `bin/<prefix>gcc`, for toolchains outside `$PATH` |
| `VP` | `../../../vp/build/bin/sep-vp` | Path to the `sep-vp` executable |
| `VP_BUILD_DIR` | `vp/build_sep` | Build directory used when the runner builds `sep-vp` |
| `DEBUG_PORT` | read from INI (`gdbTcpPort`), fallback `5005` | GDB server port |
| `EXTRA_CFLAGS` | _(empty)_ | Additional compiler flags |

Example:
```bash
EXTRA_CFLAGS="-DDEBUG_VERBOSE" make
```

---

## Writing New Tests

1. Create a new directory under `sep-vp-tests/`
2. Write `main.c` with test logic (use `printf()` from `common/printf.c` for output)
3. Create a `Makefile`:

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
make && make sim
```

Or run it from the parent directory with the runner:

```bash
cd ..
./run_sep_vp_tests.sh my-test-name
```

### `printf` Format Specifiers Available

The bare-metal `printf` in `common/printf.c` supports:

| Specifier | Output |
|-----------|--------|
| `%s` | String |
| `%u` | Unsigned decimal |
| `%x` | Hex (8 digits, no prefix) |
| `%0Nx` | Hex with N digits (e.g., `%08x`) |
| `%%` | Literal `%` |

> **Note**: `%d` is **not** supported. Use `%u` for unsigned integers and `%x` for hex register values.

---

## Additional Resources

- **Main Project README**: `../../README.md`
- **VP Configuration**: `../../vp/platform/sep/config/accellera_config.ini`
- **RISC-V ISA Manual**: https://riscv.org/technical/specifications/
- **GDB Manual**: https://sourceware.org/gdb/current/onlinedocs/gdb/
