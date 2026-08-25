# SPI Flash (external BFM)

SystemC BFM of an off-chip SPI NOR part. This is **not SEP silicon**:
the SEP die has no flash IP; firmware reaches this model only through
`spi_controller`. Architecture and CSRs of the host are in the
hardware TRM. This tree has the BFM, its test plan, and how to run
the tests.

The core (`spi_flash_model`) is pure C++. `spi_flash` is the
`sc_module` wrapper that implements `spi_if`.

## Status

| Item | State |
|---|---|
| Profile-1 command set, SFDP, WREN, suspend | Implemented |
| CSAAT-chained multi-segment reads | Implemented |
| TLM / CSRs | None (not on the bus) |
| Standalone tests | `./run_tests.sh` (C++ unit); `--ctest` also runs the SystemC wrapper |
| Wired into `sep-vp` | Bound to `spi_controller::spi_master`; optional `spiPreload` |

## Files

```
include/spi_flash_model.h         Pure C++ flash core (spi_flash_model class)
include/spi_flash_sfdp.h          SFDP table structure definitions
include/spi_flash.h               SystemC wrapper (spi_flash sc_module)
src/spi_flash_model.cpp           Command handler, program/erase/read logic
src/spi_flash.cpp                 sc_module wiring and spi_if implementation

test/inc/spi_flash_sfdp_utils.h   SFDP test utilities
test/src/spi_flash_sfdp_utils.cpp SFDP parser / pretty-printer
test/src/spi_flash_test.cpp       Pure C++ unit tests (no sc_main)
test/src/spi_flash_sc_test.cpp    SystemC integration testbench (sc_main)

doc/index.adoc                    VP set entry
doc/implementation.adoc           SystemC/TLM model
doc/test_plan.adoc                cases + run commands
```

## Class

```cpp
class spi_flash : public sc_module, public spi_if
```

## Interface

| Port / Export | Direction | Description |
|---|---|---|
| `spi_target` | `sc_export<spi_if>` | SPI transaction target (bound by spi_controller) |
| `rst_ni` | `sc_in<bool>` | Active-low reset — clears WEL, preserves memory |

## Behavior notes

**Multi-segment (CSAAT-chained) reads.** A read command is framed as one opcode+address TX segment followed by one or more RX segments; when the driver keeps CS asserted (`csaat=1`) across several RX segments, the model serves each RX segment's data from a running address that advances by each segment's length. (Previously data was served only on the final `csaat=0` segment, which truncated multi-segment reads.) A bare RX segment with no preceding opcode+address returns `0xFF` (idle MISO).

**Device identity.** `RDID` (`0x9F`) returns a 3-byte JEDEC ID defaulting to `0x20BA18`, the
same part the DV flash BFM (`ocah_spi_flash.py`) presents. `set_jedec_id()` overrides it,
mirroring the BFM's `+spi_flash_jedec_id` plusarg. The capacity byte `0x18` encodes 2²⁴
bytes, so it agrees with the 16 MB default array — density, JEDEC capacity byte and SFDP
addressing mode are deliberately kept consistent, since a mismatch between them is the kind
of thing a bootloader will trip over.

**Program wraps within its 256-byte page**, as real NOR devices do: a program crossing a page
boundary wraps to the start of the same page rather than spilling into the next one. Program
can only clear bits (1→0); erase restores a 64 KB block to `0xFF`.

**Backdoor loading is always explicit.** The model never probes the working directory for an
image. `load_memory_from_file()` takes a path, and on a platform the image is named by one of
two config keys, both resolved relative to the `.ini` that names them:

- `och_sep_ss1.spiPreload` — a Verilog `$readmemh`-style text image
- `och_sep_ss1.spiBackdoorFile` — a raw binary image

`spiPreload` wins if both are set. With neither set the flash starts erased, which is what
tests asserting `0xFF` depend on. `BACKDOOR_FILE_PATH` (`data/flash_memory.bin`) survives only
as a shared naming convention for testbenches — nothing opens it implicitly.

## Building and Testing

```bash
./run_tests.sh              # Release: spi_flash_test only
./run_tests.sh --debug
./run_tests.sh --asan       # AddressSanitizer + UBSan
./run_tests.sh --coverage   # both binaries; do not combine with --asan
./run_tests.sh --ctest      # spi_flash_test + spi_flash_sc_test
./run_tests.sh --clean
```

Platform firmware tests talk to this BFM through the SPI host. Full
commands are in `doc/test_plan.adoc`.

## Documentation

Architecture and host CSRs are in the hardware TRM. This tree
documents the BFM:

- [index](doc/index.adoc) — VP set entry
- [implementation](doc/implementation.adoc) — SystemC/TLM model
- [test plan](doc/test_plan.adoc) — standalone cases and platform runs
