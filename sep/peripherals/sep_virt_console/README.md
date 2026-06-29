# sep_virt_console — SEP bootcode "SIM_OUT" virtual console

Surfaces the SEP bootcode's `simput*` status messages on the VP host console.

The bootcode (`fw/sep/bootcode/include/rom_virt_console.h`) reports progress by writing
a packed 32-bit protocol to `SEP_SCRATCH_COLD_SCRATCH_2` (`0x10802010`). On silicon /
RTL this is decoded by a cocotb monitor (`dv/.../sep_virt_console.py`). In the
standalone-SEP virtual platform there is no such consumer, so this component observes
those writes, decodes them, and prints each reassembled line through the shared CSML
logger with a `SIM_OUT` source field — exactly the way internal VP status is printed.

> **Simulation aid, not hardware.** Real hardware has no register that "prints". This
> component is a VP observability aid and does **not** change the register's read/write
> semantics. In the combined "SMU" configuration the SMC reads this register itself, so
> this console is intended for the standalone-SEP case and can be disabled (see Config).

## Interface

- **Observation hook**: the platform injects a write-tap into the `sep_scratch`
  `SEPMemory` instance (`SEPMemory::setWriteTap`). Writes to offset `0x10`
  (`SEP_SCRATCH_COLD_SCRATCH_2`) are forwarded to `SimVirtConsole::on_bytes` /
  `on_word`. See `vp/platform/sep/och_sep_ss.hpp`.
- **Output**: STDOUT (and the CSML global log file when configured), one line per
  newline-terminated message:
  `[<time>] [INFO <v>] [SIM_OUT] - <decoded text>`

## Protocol (decoded; mirrors the cocotb decoder for parity)

| opcode (`bits[3:1]`) | meaning | payload | rendered |
|----------------------|---------|---------|----------|
| 0 ASCII | up to 3 bytes at `[15:8]`,`[23:16]`,`[31:24]`, NUL-stop | text | the characters |
| 1 HEX16 | `[23:8]` | 16-bit | 4 lowercase hex digits |
| 2 DEC24 | `[31:8]` | 24-bit | decimal |

Bit `[0]` is a duplicate-detection toggle and is ignored by decode. `simputhex32` is
`"0x"` + two HEX16 writes (high half first) → `0xHHHHLLLL`. Lines end at a `'\n'` in the
ASCII stream; the trailing buffer is flushed at end of simulation.

## Configuration (CCI)

| param | default | effect |
|-------|---------|--------|
| `och_sep_ss1.sim_out.enable` (bool) | `true` | master on/off; when false, no tap is installed and no `SIM_OUT` lines are produced |
| `och_sep_ss1.sim_out.verbosity` (int) | `2` | CSML verbosity gate for the emitted lines |

## Build & test

```bash
./run_tests.sh            # Release build + CTest (self-checking decoder tests)
./run_tests.sh --asan     # AddressSanitizer
./run_tests.sh --clean    # rebuild from scratch
```

The decode logic lives in the SystemC-free `VirtConsoleDecoder` (`include/virt_console_decoder.h`)
so it is unit-testable in isolation; `SimVirtConsole` (`include/sep_virt_console.h`) is the
thin SystemC wrapper that adds CCI config and CSML emission.

## Files

- `include/virt_console_decoder.h` — pure decode state machine (correctness oracle)
- `include/sep_virt_console.h`, `src/sep_virt_console.cpp` — SystemC wrapper (`SimVirtConsole`)
- `test/src/virt_console_decoder_test.cpp` — self-checking CTest cases
- `doc/01_…Specification.md`, `doc/02_…LowLevel_Design.md`, `doc/03_…Test_Plan.md`
