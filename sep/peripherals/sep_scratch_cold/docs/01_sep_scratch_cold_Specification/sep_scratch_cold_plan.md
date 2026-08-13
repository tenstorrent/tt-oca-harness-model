# sep_scratch_cold / sep_scratch_warm — Model Plan

Both blocks share the same RDL (`sep_scratch.rdl`): 8 × 64-bit plain scratch registers.
The only difference in hardware is the reset domain; in the VP both behave identically.
This plan covers both. The warm plan is a short stub pointing here.

---

## 1. What Are These Units?

| Block | Base | Reset behavior |
|---|---|---|
| `sep_scratch_cold` | `0x1080_2000` | Survives warm reset; cleared on cold (power-on) reset only |
| `sep_scratch_warm` | `0x1080_2080` | Cleared on any reset (warm or cold) |

Both are **plain read/write scratch registers** — no hardware consumes their values, no
side-effects on reads. Firmware uses them for:
- Boot-stage inter-stage communication (save state across warm reset)
- Debug logging via the **virtual console protocol** (`sep_scratch_cold.SCRATCH[2]` only)
- General-purpose temp storage

**Cold vs warm in the VP:** The VP has a single reset domain. At simulation start both blocks
are zeroed. The "cold survives warm reset" distinction does not apply — both are treated as
plain R/W storage. This is correct for all current firmware tests.

---

## 2. Register Layout (identical for both blocks, from `sep_scratch.rdl`)

8 instances of `SCRATCH` register, packed at stride `0x08`:

| Offset | Register | Field | Bits | Access | Reset |
|---|---|---|---|---|---|
| `0x00` | `SCRATCH[0]` | `data` | [31:0] | sw=rw, hw=r | `0x0` |
| `0x08` | `SCRATCH[1]` | `data` | [31:0] | sw=rw, hw=r | `0x0` |
| `0x10` | `SCRATCH[2]` | `data` | [31:0] | sw=rw, hw=r | `0x0` |
| `0x18` | `SCRATCH[3]` | `data` | [31:0] | sw=rw, hw=r | `0x0` |
| `0x20` | `SCRATCH[4]` | `data` | [31:0] | sw=rw, hw=r | `0x0` |
| `0x28` | `SCRATCH[5]` | `data` | [31:0] | sw=rw, hw=r | `0x0` |
| `0x30` | `SCRATCH[6]` | `data` | [31:0] | sw=rw, hw=r | `0x0` |
| `0x38` | `SCRATCH[7]` | `data` | [31:0] | sw=rw, hw=r | `0x0` |

Total size: `0x40` bytes per block. `regwidth=64; accesswidth=64` — register is 64 bits wide
but only the lower 32 bits (`data[31:0]`) are implemented; upper 32 bits read back as 0.

---

## 3. SCRATCH_COLD[2] Virtual Console — the one functional behavior

`SCRATCH_COLD.SCRATCH[2]` (absolute address `0x1080_2010`) is decoded by the simulation
ROM as a **virtual console output port**. Firmware built with `#define DEBUG` uses
`rom_virt_console.h` to print ASCII strings, hex values, and decimal values by packing
them into 32-bit writes to this register.

**Protocol (from `rom_virt_console.h`):**

```
[31:8]  payload
[ 7:4]  reserved (always 0)
[ 3:1]  opcode
[   0]  toggle bit (flips when consecutive values are identical, used for de-dup in RTL monitors)
```

| Opcode | Name | Payload layout |
|---|---|---|
| `0` | ASCII | 3 characters in bytes [15:8], [23:16], [31:24] |
| `1` | HEX16 | 16-bit value in bits [23:8] |
| `2` | DEC24 | 24-bit value in bits [31:8] |

The toggle bit is for RTL waveform monitors (to detect re-writes of the same value).
In the VP model the toggle bit is ignored — the model decodes on every write.

**Only `sep_scratch_cold.SCRATCH[2]` carries this protocol.** All other SCRATCH registers
in both cold and warm blocks are plain R/W with no side-effects.

---

## 4. Model Code Assessment (`sep_scratch_cold.h`)

> **Superseded.** This section assesses a `sep_scratch_device` that covered cold and warm
> as one contiguous 48-word device. No such module exists: the implementation splits them
> into `sep_scratch_cold_ip` and `sep_scratch_warm_ip`, each with its own register bank and
> `rst_ni`, so the cold-domain side effects stay out of the warm block and the two can sit
> in different reset domains. The addresses and the SCRATCH[2] console attachment below
> still hold; the single-module layout does not. Kept for the reasoning that led here.

### What the superseded single-module design did

`sep_scratch_device` was a single `sc_module` covering **both cold and warm** as one
contiguous device (`REG_WORDS = 0xC0/4 = 48` 32-bit words spanning `0x10802000–0x108020BF`).
The layout in the word array:

| Byte offset | Content |
|---|---|
| `0x00–0x3F` | SCRATCH_COLD[0..7] |
| `0x40–0x7F` | Gap (reserved in hardware; reads 0, writes stored but unused) |
| `0x80–0xBF` | SCRATCH_WARM[0..7] |

`SCRATCH2_IDX = 0x10/4 = 4` — correct word index for SCRATCH_COLD[2].

### What is correct

- Virtual console decode (`decode_vconsole`) — correct: opcode from `[3:1]`, toggle bit `[0]`
  ignored, ASCII / HEX16 / DEC24 paths all correct.
- Plain R/W storage for all other registers.
- Graceful handling: out-of-range accesses return `TLM_OK_RESPONSE` with 0 data.
- `transport_dbg` delegates to `b_transport`.
- Only handles `data_length == 4` (32-bit accesses) — correct for firmware `sw`/`lw`.

### One issue to verify — address handling

The model computes:
```cpp
unsigned idx = static_cast<unsigned>(trans.get_address()) / 4;
```

This assumes `trans.get_address()` is already an **offset from the device base** (i.e., 0x00–0xBF),
not the full physical address (`0x10802000+`). If the SimpleBus passes full physical addresses to
the target socket, `idx` would be `0x10802000/4 ≈ 67M` — far beyond `REG_WORDS = 48`, and all
accesses would silently no-op.

**Verify before integration:** check how the VP SimpleBus delivers addresses to target sockets
(offset or full physical). If full physical, add `trans.get_address() - BASE_ADDR` with
`BASE_ADDR = 0x10802000`.

---

## 5. VP Model Scope

### In scope — model is functionally complete as-is (pending address verification)
- Plain R/W storage for all 16 SCRATCH registers (cold + warm).
- Virtual console on `SCRATCH_COLD[2]` — the only meaningful VP behavior.
- Single device covers both blocks; no functional split needed.

### Platform wiring

```cpp
sep_scratch_device scratch("scratch");
bus.add_target(scratch.sock, 0x10802000, 0x108020C0);
```

The single socket covers both cold and warm; the 0x40-byte gap between them is handled
gracefully (reads 0, writes discarded).

### What is not modeled
- Cold/warm reset-domain distinction — VP has one reset domain; both blocks reset to 0 at sim start.
- Upper 32 bits of each 64-bit register — always 0; no firmware uses them.
