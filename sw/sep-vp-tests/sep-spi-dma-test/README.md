# SEP SPI-controller → secure-DMA streaming integration test

Bare-metal SEP test that exercises the **SPI controller and the secure DMA working
together** on the platform — the integration surface that the per-model unit suites can
only cover against a mock of the counterpart, and that otherwise is only reached by the
full firmware boot.

It runs several back-to-back hardware-handshake DMA drains of flash into SRAM, exactly
the way the boot ROM frames each read:

```
for each read:
  CTRL.SW_RST                        flush the controller
  arm secure DMA (hardware handshake): SRC = fixed SPI RXDATA FIFO,
                                       DST = incrementing SRAM buffer,
                                       CHUNK = RX watermark, GO|INITIAL|HSHAKE
  TX  opcode(0x03) + 24-bit address  (CSAAT held)
  RX  CSAAT-chained data segments    (≤ RX-FIFO sized; last releases CS)
  poll DMA STATUS.done
```

The controller streams the RX data while the DMA drains one chunk per RX-watermark
trigger (`spi_controller.dma_trigger` → `secure_dma.lsio_trigger[0]`); the controller
stalls SCK when the FIFO fills, so an over-FIFO segment cannot overflow it.

## What it guards against

Running **several** such reads with a `CTRL.SW_RST` before each is the pattern that
exposed the *second-read TX-command drop*: after a read completes, the next read's
`SW_RST` lands while the previous transaction is still finishing its tail segment, and a
stale post-process pop in the controller discarded the freshly queued opcode+address TX
command. The dropped command left the flash undriven, so the read returned wrong data.

The test is deliberately **timing-faithful** to reproduce that window:

- It gates the next read on **DMA-done only** (as the firmware does), not on the
  controller going idle. The controller pushes (and the DMA drains) the last segment's
  data *before* that segment's bit-clock delay elapses, so DMA-done arrives with the FSM
  still `ACTIVE` — and the next `SW_RST` overlaps it.
- The reads run back-to-back in one pass with **no work between them**; verification is a
  separate second pass. Doing per-transfer `printf`/verify inline would burn more
  simulated time than the tail-segment delay, so the next `SW_RST` would land after the
  controller went idle and the regression window would be missed (a false PASS).

Verified bug-sensitive: against a controller with the fix reverted, transfer 0 passes but
transfers 1–3 fail (≈510/512 bytes wrong); with the fix, all transfers pass.

## Data oracle

The staged flash image is `byte[i] = i & 0xFF` (see `gen_flash_fixture.py`), so a correct
DMA of `len` bytes from flash offset `off` must land `(off + k) & 0xFF` at destination
byte `k`. The four reads use offsets with distinct low bytes (0x40, 0x60, 0x80, 0xA0) so a
dropped read cannot be masked by stale buffer contents. This single check covers
addressing, ordering, and completeness.

## Self-contained

All register addresses, fields, and the DMA programming model are hardcoded in `main.c`
(matching the SEP platform memory map). The test builds and runs with only the RISC-V
toolchain and the in-tree `sep-vp` — it does **not** depend on the tt-oca-hw hardware repo
or its generated headers.

## Build & run

```bash
# From this directory, with the RISC-V toolchain on PATH and gcc-toolset-11 active
# (sep-vp needs the gcc-11 C++ runtime):
make sim-staged      # stage the fixture, build, run on sep-vp, unstage
make distclean       # remove build artifacts + the staged fixture
```

`make sim-staged` stages the fixture to `<config-dir>/data/flash_memory.bin` (sep-vp
`chdir`s to the config `.ini` directory, so the flash model resolves it there), runs the
in-tree `sep-vp`, then unstages. The firmware self-checks and prints:

```
SEP_SPI_DMA_TEST: ALL PASS
All tests PASSED!
```

An automated wrapper lives at `tools/virtual_platform/tests/test_spi_dma_stream.py`
(mirrors `test_spi_mux_flash.py`): it drives `make sim-staged` and asserts the pass marks,
skipping cleanly if the RISC-V toolchain or `sep-vp` is absent.
