# 01 — sep_virt_console Specification

## Purpose

Decode the SEP bootcode "virtual console" protocol written to
`SEP_SCRATCH_COLD_SCRATCH_2` (`0x10802010`) and emit each reassembled line to the
virtual-platform host console with a `SIM_OUT` header, the same way the platform's own
status messages are printed.

This is a **simulation observability aid** for the standalone-SEP configuration. It
reproduces what the firmware's `simput*` helpers
(`fw/sep/bootcode/include/rom_virt_console.h`) intend to print; the cocotb monitor
`dv/sep/tb/tb_uvm/cocotb_tests/common/sep_virt_console.py` performs the equivalent decode
for RTL runs. The component models no hardware behavior and does not change register
semantics.

## Inputs

Firmware writes a packed 32-bit word to the scratch register:

```
[31:8] payload   [7:4] reserved   [3:1] opcode   [0] toggle
```

| opcode | name  | payload                                   | render |
|--------|-------|-------------------------------------------|--------|
| 0      | ASCII | ≤3 bytes [15:8],[23:16],[31:24], NUL-stop | characters |
| 1      | HEX16 | 16-bit value [23:8]                       | `%04x` (lowercase) |
| 2      | DEC24 | 24-bit value [31:8]                       | decimal |
| 3–7    | —     | —                                         | ignored (no-op) |

- Bit `[0]` (toggle) distinguishes back-to-back identical firmware writes; it is **not**
  part of the opcode/payload and is masked out by decode.
- A logical line ends at a literal `'\n'` in the ASCII stream (no end-of-line opcode).
- `simputhex32` is sent as `"0x"` (ASCII) + HEX16(high) + HEX16(low) → `0xHHHHLLLL`.

## Outputs

One STDOUT line per newline-terminated message (and to the CSML global log file when one
is configured):

```
[<sim-time>] [INFO <verbosity>] [SIM_OUT] - <decoded text>
```

Any buffered, unterminated content is flushed as a final line at end of simulation.

## Behavior

- Observes every write to the register and decodes it.
- Reassembles ASCII messages that span multiple writes.
- Renders HEX16/DEC24 (and the two-part hex32) with the value the firmware reported.
- Ignores the toggle bit, so repeated identical messages each print once — never dropped,
  never duplicated.
- Emits through the shared CSML logging facility on STDOUT with the `SIM_OUT` header; it
  never writes STDERR.
- Emits one line per newline and flushes any trailing buffer at end of simulation.
- Observation only — the register's read/write behavior for firmware is unchanged.
- Enabled by default; can be disabled at runtime (`och_sep_ss1.sim_out.enable`).
- Handles unknown opcodes and sub-word writes gracefully (never aborts or hangs).
- Produces deterministic output for a given firmware image and configuration.

## Scope

Standalone SEP. In the combined SMU configuration the SMC reads the register itself, so
this console is intended for standalone SEP and can be disabled. SMC-side handling is out
of scope.
