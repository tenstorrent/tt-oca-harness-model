# SPI Controller

SystemC TLM-2.0 loosely-timed model of the SEP SPI host. Architecture,
CSRs, and programming sequences are in the hardware TRM. This tree has
the model, its test plan, and how to run the tests. The off-chip NOR
part is the `spi_flash` BFM, not this IP.

## Status

| Item | State |
|---|---|
| Register file + CMD/STATUS handshake | Implemented |
| Standard / Dual / Quad segments, CSAAT, FIFOs | Implemented |
| Interrupts, DMA trigger, SW_RST abort | Implemented |
| Pass-through / SPI_DEVICE mux | Not modelled |
| Standalone tests | `./run_tests.sh` |
| Wired into `sep-vp` | SPI window, PIC event IRQ, DMA LSIO 0, `spi_flash` |

## Files

```
include/spi_controller.h            spi_controller_ip
include/spi_controller_base.h       CSML register declaration
include/spi_controller_register.h   RO/WO/RW types
include/spi_controller_interface.h  spi_if / segment types
src/                                LT implementation
test/                               standalone bench
doc/implementation.adoc             SystemC/TLM model
doc/test_plan.adoc                  cases + run commands
```

## Building and testing

```bash
./run_tests.sh              # Release build + run
./run_tests.sh --debug
./run_tests.sh --asan       # AddressSanitizer + UBSan
./run_tests.sh --coverage   # do not combine with --asan
./run_tests.sh --ctest
./run_tests.sh --clean
```

Platform firmware tests on `sep-vp`:

```bash
cd sw/sep-vp-tests/fw-tests-from-tt-oca-hw/fw/sep
./run_test.sh spi_ot_reg_test
./run_test.sh spi_ot_flash_read_test
./run_test.sh spi_ot_interrupt_test
```

Those need a built `sep-vp` and a RISC-V bare-metal toolchain. Full
commands are in `doc/test_plan.adoc`.
