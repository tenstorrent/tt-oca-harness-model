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
make                 # build
make sim             # build if needed, then run on sep-vp
make clean           # remove build artifacts, the staged fixture and the derived config
make distclean       # the above, plus the generated fixture image
```

`make` and `make sim` behave the same as every other test in `sw/sep-vp-tests`. As with
those, `sep-vp` does not self-terminate: press `Ctrl-C` once the summary prints. The
firmware self-checks and reports:

```
SEP_SPI_DMA_TEST: ALL PASS
All tests PASSED!
```

`make sim` prepares two things first, both into the `sep-vp` config directory, because the
VP `chdir`s to the directory of the `.ini` it is given and resolves everything relative to
it:

- `data/flash_memory.bin` — the staged fixture, so the flash model has the data oracle.
- `accellera_config_no_spipreload.ini` — the shared `accellera_config.ini` with
  `och_sep_ss1.spiPreload` commented out. That setting loads the bootcode image directly
  into the flash model's backing store, overwriting the fixture; with it enabled all four
  transfers fail against boot data. The config is derived with `sed` at build time instead
  of being checked in, so it picks up any edits to the shared config. The rule lives in
  `../Makefile.common` as `NO_SPIPRELOAD_INI`; `sep-spi-mux-test` uses it too.

Both are build artifacts, are git-ignored, and are removed by `make clean`.
