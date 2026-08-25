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
include/spi_controller_interface.h    spi_if and segment type definitions
include/spi_controller_register.h     Register type definitions
include/spi_controller_base.h         Register map and TLM socket base
include/spi_controller.h              spi_controller_ip class declaration
src/spi_controller_base.cpp           Base construction and register binding
src/spi_controller.cpp                Transaction engine and b_transport handler

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

doc/index.adoc                        VP set entry
doc/implementation.adoc               SystemC/TLM model
doc/test_plan.adoc                    cases + run commands
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
| `irq_o` | `sc_out<bool>` | Combined interrupt — the pin the PIC sees |
| `error_irq` | `sc_out<bool>` | Error conditions (CMDBUSY, OVERFLOW, UNDERFLOW, CMDINVAL, CSIDINVAL, ACCESSINVAL) |
| `spi_event_irq` | `sc_out<bool>` | Event conditions (IDLE, READY, TXEMPTY, RXFULL, TXWM, RXWM) |
| `dma_trigger` | `sc_out<bool>` | DMA request on watermark crossing |
| `rst_ni` | `sc_in<bool>` | Active-low reset |

The IP has **one** interrupt pin in silicon, `irq_o = error_intr || spi_event_intr`, and that
is the port SEP binds to its PIC slot. `error_irq` and `spi_event_irq` expose the two classes
separately for observability only — nothing consumes them on a platform, and wiring them to
separate PIC sources would model a device that does not exist.

## Behavior notes

**RX back-pressure streaming.** An RX segment larger than the free RX-FIFO space is accepted and streamed rather than rejected: the controller fills the FIFO to capacity, stalls (modeling the serial clock holding off while the FIFO is full), and resumes as the drainer (CPU reads of `RXDATA`, or the secure-DMA) pops words. A segment is only rejected if it exceeds the model's per-transaction buffer (512 bytes). `dma_trigger` is asserted while `rx_qd >= rx_watermark` (or `tx_qd < tx_watermark`), so a drainer sees a rising edge each time the FIFO refills past the watermark and a falling edge as it drains below.

**Software reset aborts in-flight transactions.** `CTRL.SW_RST` flushes the FIFOs, clears the command queue, and cleanly aborts a transaction that is currently stalled waiting for RX-FIFO space (back-pressure) or for TX-FIFO data, returning the engine to idle. It samples whether a transaction is in flight (FSM `ACTIVE`) *before* forcing the FSM idle, so the abort is armed only when there is really something to abort.

**Back-to-back reads across a reset never drop the next command.** Firmware frames each flash read as `SW_RST` → push opcode+addr → TX command (`CSAAT`) → chained RX segments, and issues the next read's commands immediately. Because processing a segment consumes simulation time (bit-clock delay, back-pressure waits), a `SW_RST` and the next read's TX command can land while the previous read's tail segment is still completing. The transaction thread reads the queue front, processes it, then pops — but if a `SW_RST` cleared the queue mid-flight, the current front is now the *newly queued* command, not the segment just processed. The reset's abort flag suppresses that one stale pop (and is cleared at the next segment's processing entry), so the freshly queued opcode+address TX command is preserved and driven, rather than being silently discarded. This keeps `STATUS.CMDQD` accounting intact during processing (the in-flight segment stays counted until it legitimately completes).

**FIFO status and watermark semantics.** `STATUS.TXFULL` asserts at `size >= TxDepth + 1`,
because the RTL counts the word held in the shift register alongside the FIFO contents.
`STATUS.RXWM` is a plain `rx_qd >= rx_watermark` comparison with no "watermark is non-zero"
guard, so a watermark of zero reads as permanently met — that is what the hardware does. The
`dma_trigger` output is the exception and keeps a non-empty term, since a DMA request on an
empty FIFO would be meaningless.

**`INTR_STATUS` is read-only to software.** Status bits are set and cleared by the hardware
condition; software cannot poke them. The interrupt level is computed from one equation and
there is no separate edge-triggered path, so the level equation is the only writer.

**`ACCESSINVAL` escalates unconditionally**, matching the RTL's `error_mask`, rather than
being suppressed when the corresponding enable is clear.

**A CMD write is accepted while an error is latched.** A pending error does not gate command
acceptance; a full command queue is the only reason a CMD write is refused. Firmware
recovering from an error does not have to clear it first.

## Building and Testing

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

## Documentation

Architecture, CSRs, and programming sequences are in the hardware
TRM. This tree documents the SystemC/TLM model:

- [index](doc/index.adoc) — VP set entry
- [implementation](doc/implementation.adoc) — sockets, threads, CCI, gaps
- [test plan](doc/test_plan.adoc) — standalone cases and platform runs
