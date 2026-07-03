# 02 — sep_virt_console Low-Level Design

## Components

```
            sep_scratch (SEPMemory)                 SimVirtConsole (sc_module)
            ┌───────────────────────┐               ┌──────────────────────────────┐
 CPU write  │ b_transport()         │  setWriteTap  │ on_bytes()/on_word()         │
 ─────────► │  writeBytes()         │ ────tap────►  │   └─ VirtConsoleDecoder      │
            │  if(tap) tap(off,d,l) │  (off==0x10)  │        on_word() → emit_(line)│
            │  get_direct_mem_ptr=  │               │ emit_ = CSML_INFO(2)[SIM_OUT] │
            │   false when tapped   │               │ ~dtor → decoder_.flush()      │
            └───────────────────────┘               └──────────────────────────────┘
```

### `VirtConsoleDecoder` (`include/virt_console_decoder.h`) — pure C++, no SystemC

State: `cur_line_` (std::string), `enabled_` (bool), `emit_` (callback).

- `on_word(uint32_t)`: `op = (word>>1)&0x7` (masks toggle bit[0]); dispatch ASCII/HEX16/
  DEC24; unknown opcode → no-op. Disabled → no-op.
- `on_bytes(data,len)`: assemble little-endian word; `len<4` or null → ignored (sub-word
  guard); else `on_word`.
- ASCII: bytes `[15:8],[23:16],[31:24]`, stop at NUL, `append_char`.
- HEX16/DEC24: `snprintf` then `append_char` per digit.
- `append_char('\n')` → emit `cur_line_` and clear; else accumulate.
- `flush()`: emit non-empty `cur_line_` (end-of-sim).

Keeping decode free of SystemC makes it the unit-test oracle and lets parity with the
cocotb decoder be tested without a simulator.

### `SimVirtConsole` (`include/sep_virt_console.h`, `src/sep_virt_console.cpp`) — sc_module

- CCI params: `csml_param<int> verbosity`, `csml_param<bool> enable`.
- Constructor: `logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [SIM_OUT] - %MESSAGE%")`
  — `SIM_OUT` is a literal field because `%MODULE%` resolves to the SystemC instance path
  and cannot be forced to a fixed token; sets decoder `enabled` from the param; the `emit_`
  lambda does `CSML_INFO(2, logger) << line` (level 2 → visible at default verbosity;
  LogStream adds the newline).
- `on_word`/`on_bytes` delegate to the decoder.
- Destructor flushes the decoder so trailing content is not lost.

### SEPMemory hook (`sep/peripherals/sep_memory/…`)

- `using WriteTap = std::function<void(uint64_t,const uint8_t*,unsigned)>;` + `setWriteTap`.
- In `b_transport` write path, after `writeBytes`, `if (m_write_tap) m_write_tap(addr,ptr,len)`
  (addr is region-local; observation only).
- `get_direct_mem_ptr` returns `false` when a tap is set (deny DMI → every write traps).

### Platform wiring (`vp/platform/sep/och_sep_ss.hpp`)

- Declare/create/`delete` a `SimVirtConsole* sim_out`.
- In `module_bind`, if `sim_out->enabled()`, install the tap on `sep_scratch` filtering
  offset `0x10` and `len>=4`, forwarding to `sim_out->on_bytes`.

## Design rationale

- **Tap on SEPMemory**, not the bus write-observer (which carries no data payload) and
  not a forwarding bridge (extra socket hop and DMI handling for no benefit).
- **Literal `SIM_OUT`** via the CSML format string, so the header is stable and greppable
  regardless of instance hierarchy.
- **Destructor flush**, matching the codebase's teardown style (no `end_of_simulation()`
  use elsewhere).

## Determinism / performance

- O(bytes written) on a low-frequency register; loosely-timed only.
- Output depends only on the firmware writes and configuration; `%TIME%` reflects
  deterministic simulated time.
