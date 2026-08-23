# SPI Flash Model Test Plan

## Document Information

| Field | Value |
|---|---|
| IP | SPI NOR Flash device model |
| Model classes | `spi_flash_model` (pure C++), `spi_flash` (`sc_module`) |
| Test binaries | `spi_flash_test` (no simulator), `spi_flash_sc_test` (SystemC) |
| Reference | `ocah_spi_flash.py`, JESD216A |

## 1. Strategy

The model is split in two, and so is its verification. This is the main structural decision
in the plan and it is worth stating plainly:

- **`spi_flash_test`** exercises `spi_flash_model` directly, with no simulator involved. It
  covers the device: command handling, flash semantics, SFDP construction and parsing, the
  backdoor. Because it links no SystemC, it builds and runs in a fraction of the time, so it
  carries the bulk of the cases.
- **`spi_flash_sc_test`** exercises the `sc_module` wrapper through the `spi_if` interface. It
  covers what only exists once segments and signals are involved: segment framing, CSAAT
  chaining, and the reset port.

A case belongs in the SystemC suite only if it needs a simulator. Everything else goes in the
pure C++ suite.

## 2. Coverage areas

| Area | Suite | Notes |
|---|---|---|
| SFDP DWORD field encoding | C++ | One case per DWORD, 1 through 16 |
| SFDP ROM construction and parsing | C++ | Including end-to-end and edge cases |
| Blank-device read | C++ | Erased array reads `0xFF` |
| WREN / WRDI | Both | Latch set and cleared |
| Program semantics | C++ | Bits clear only; program without WREN fails |
| Page-program wrap | C++ | A program crossing a page boundary wraps within the page |
| Erase, 64 KB | C++ | Erase without WREN fails |
| Chip erase guards | C++ | |
| Missing erase granularities | C++ | Opcodes the part does not implement |
| Suspend / resume | Both | |
| Reset (command) | C++ | Arm with `0x66`, execute with `0x99` |
| Reset (signal) | SystemC | `rst_ni`, and that memory survives it |
| 4-byte addressing | Both | `EN4B` / `EX4B` |
| JEDEC ID | Both | Default `0x20BA18` and override |
| Unknown opcode | Both | Returns failure |
| Large density | C++ | Non-default array size |
| Backdoor read / write | C++ | Direct byte access |
| Backdoor file I/O | C++ | Explicit path, round-trip |
| Segment framing, one and two segments | SystemC | |
| Pure RX segment with no command | SystemC | Returns `0xFF` |

## 3. Behaviours a test must not assume

Four properties follow from the model's abstraction level or from RTL alignment, and tests
written from general SPI-flash knowledge tend to get them wrong.

1. **There is no WIP busy bit.** Operations complete instantly, so a poll loop waiting for
   busy-then-idle will never see busy. Do not assert a busy transition; assert the result.

2. **A blank flash reads `0xFF`, and nothing is loaded implicitly.** The model does not probe
   the working directory for an image. If a test needs content it must name it — through
   `spiBackdoorFile`, through `spiPreload`, or by calling `load_memory_from_file()` with a
   path. A test asserting `0xFF` is now safe from a stray file in the run directory, which was
   not true before this was made explicit.

3. **Program wraps within its page.** A 256-byte page program starting mid-page wraps to the
   start of that page. It does not spill into the next page, and it does not truncate.

4. **`RDID` returns `0x20BA18` unless overridden**, and the capacity byte `0x18` must stay
   consistent with the array size. A test that changes one should change the other; the two
   disagreeing is a device that could not exist.

## 4. Platform-level coverage

Three firmware tests exercise the flash through the full controller and bus path, which is
where framing and configuration bugs actually surface:

| Test | What it proves |
|---|---|
| `sep-spi-test` | Controller-to-flash reads against an erased device |
| `sep-spi-dma-test` | DMA-driven reads from a staged fixture, via `spiBackdoorFile` |
| `sep-spi-mux-test` | Mux control register plus staged and unstaged flash reads |

The last two stage their own fixture and generate a per-test `.ini` naming it through
`och_sep_ss1.spiBackdoorFile`. Both the fixture and the derived `.ini` are build artifacts,
git-ignored, and removed by `make clean`. Note that the value must be JSON-quoted in the
`.ini` — CCI rejects the file otherwise, and the error points at the parse, not at the path.

## 5. Running

```bash
./run_tests.sh              # both suites
./run_tests.sh --coverage   # lcov report
./run_tests.sh --asan       # AddressSanitizer
```

---

**Related:** [High-Level Design](02_spi_flash_HighLevel_Design.md) ·
RTL comparison in `md_files/SPI_FLASH_RTL_VS_VP.md`
