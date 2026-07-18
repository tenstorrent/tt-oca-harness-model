# spi_controller

SystemC TLM2.0 model of the SPI master controller.  Paired with `spi_flash` to form the subsystem.  Firmware programs transfer descriptors into memory-mapped registers; the controller drives the `spi_if` socket to execute SPI transactions on the attached flash device.

## Files

```
model/inc/spi_controller_interface.h  spi_if and segment type definitions
model/inc/spi_controller_base.h       Register map and TLM socket base
model/inc/spi_controller.h            spi_controller_ip class declaration
model/src/spi_controller_base.cpp     Base construction and register binding
model/src/spi_controller.cpp          Transaction engine and b_transport handler

test/inc/testbench.h                  Testbench module header
test/inc/spi_controller_basetest.h    Base test class
test/inc/spi_controller_test.h        Test case declarations
test/src/testbench.cpp                sc_main entry
test/src/spi_controller_basetest.cpp  Common test infrastructure
test/src/spi_controller_test.cpp      Test orchestration
test/src/test_func000.cpp             }
  ...                                 } Functional test cases (11 total)
test/src/test_func010.cpp             }
test/src/test_coverage.cpp            Coverage-oriented regression
```

## Address

`0x10B00000 – 0x10B00037`  (0x38 bytes, SPI_CONTROLLER_REG)

## Class

```cpp
class spi_controller_ip : public spi_controller_base
```

## Interface

| Port / Socket | Direction | Description |
|---|---|---|
| `target_socket` | target | TLM-2.0 32-bit register bus |
| `spi_port` | `sc_port<spi_if>` | SPI transaction interface to flash |
| `intr_o` | `sc_out<bool>` | Transfer complete interrupt |
| `rst_ni` | `sc_in<bool>` | Active-low reset |

## Behavior notes

**RX back-pressure streaming.** An RX segment larger than the free RX-FIFO space is accepted and streamed rather than rejected: the controller fills the FIFO to capacity, stalls (modeling the serial clock holding off while the FIFO is full), and resumes as the drainer (CPU reads of `RXDATA`, or the secure-DMA) pops words. A segment is only rejected if it exceeds the model's per-transaction buffer (512 bytes). `dma_trigger` is asserted while `rx_qd >= rx_watermark` (or `tx_qd < tx_watermark`), so a drainer sees a rising edge each time the FIFO refills past the watermark and a falling edge as it drains below.

**Software reset aborts in-flight transactions.** `CTRL.SW_RST` flushes the FIFOs, clears the command queue, and cleanly aborts a transaction that is currently stalled waiting for RX-FIFO space (back-pressure) or for TX-FIFO data, returning the engine to idle. It samples whether a transaction is in flight (FSM `ACTIVE`) *before* forcing the FSM idle, so the abort is armed only when there is really something to abort.

**Back-to-back reads across a reset never drop the next command.** Firmware frames each flash read as `SW_RST` → push opcode+addr → TX command (`CSAAT`) → chained RX segments, and issues the next read's commands immediately. Because processing a segment consumes simulation time (bit-clock delay, back-pressure waits), a `SW_RST` and the next read's TX command can land while the previous read's tail segment is still completing. The transaction thread reads the queue front, processes it, then pops — but if a `SW_RST` cleared the queue mid-flight, the current front is now the *newly queued* command, not the segment just processed. The reset's abort flag suppresses that one stale pop (and is cleared at the next segment's processing entry), so the freshly queued opcode+address TX command is preserved and driven, rather than being silently discarded. This keeps `STATUS.CMDQD` accounting intact during processing (the in-flight segment stays counted until it legitimately completes).

## Building and Testing

```bash
# Using run_tests.sh (recommended)
./run_tests.sh              # Release build + run
./run_tests.sh --debug      # Debug build
./run_tests.sh --asan       # AddressSanitizer
./run_tests.sh --coverage   # lcov coverage report
./run_tests.sh --ctest      # via CTest (verbose)
./run_tests.sh --clean      # clean build dir first
./run_tests.sh --cppcheck   # for static analysis

# Manual CMake
mkdir -p build/debug && cd build/debug
cmake ../.. -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
./bin/spi_controller_test
```

## Documentation

[High-Level Design](docs/design-docs/spi_controller-high-level-design.md)
