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
include/spi_flash.h            sc_module + spi_if
include/spi_flash_model.h      pure C++ command handler
include/spi_flash_sfdp.h       JESD216A tables
src/                           wrapper + model
test/                          C++ unit + SystemC bench
doc/implementation.adoc        SystemC/TLM model
doc/test_plan.adoc             cases + run commands
```

## Building and testing

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
