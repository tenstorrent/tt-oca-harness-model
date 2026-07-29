# SMC Telemetry Receiver — SystemC / TLM-2.0 Loosely-Timed Model

A **telemetry receiver** modeled in Accellera SystemC 2.3.x / 3.0.x + TLM-2.0
(Loosely-Timed). It implements one instance of the SMC telemetry receiver — the
sink of the SoC telemetry path, which re-assembles counter samples arriving over
an **ATB** (AMBA Trace Bus) byte stream and exposes them to software one message
at a time — as described in:

- `doc/01_TELEMETRY_RECEIVER_Specification.md` — externally-observable behaviour
- `doc/02_TELEMETRY_RECEIVER_LowLevel_Design.md` — TLM interface, register map, internals
- `doc/03_TELEMETRY_RECEIVER_Test_Plan.md` — unit verification strategy and test list
- `hw/comp/telemetry_receiver/data/registers/rdl/telemetry_receiver.rdl` — authoritative register map
- `hw/comp/telemetry_receiver/rtl/telemetry_receiver.sv` — behaviour reference
- `hw/comp/telemetry_receiver/rtl/telemetry_receiver_pkg.sv` — ATB packet / block widths
- `hw/periph/telemetry_receiver_wrap/data/registers/rdl/telemetry_receiver_wrap.rdl` — `NUM_TELEMETRY_RECEIVERS = 3`, `0x100` stride

The model is a drop-in `SC_MODULE` that the rest of the SMC SystemC IP library
wires up as specified in the low-level design. It is a **functional** model: the
register map, ATB packet/message decode, the circular message buffer with its
drop-oldest overflow policy, and both interrupt sources are reproduced faithfully
(and are bit-compatible with the RDL register layout), while the ATB bus
protocol, the transmitter, and cycle-level timing are abstracted away.

---

## What a telemetry receiver does

A telemetry transmitter elsewhere in the SoC samples hardware counters and
streams them to the receiver as **byte beats** on an ATB interface. The receiver:

1. **assembles** 8 beats into a 64-bit packet, and one or more packets into a
   **message** — a 5-bit probe ID plus up to 32 counter samples, each with a
   valid bit;
2. **queues** completed messages in a circular buffer (`buffer_depth` entries).
   On overflow the **oldest** message is dropped, so the freshest telemetry
   always survives;
3. **presents the oldest queued message** through read-only registers
   (`TELEMETRY_PROBE_ID`, `TELEMETRY_COUNTER_VLDS`, `TELEMETRY_COUNTER[0..31]`),
   which software drains by pulsing `CTRL.BUFFER_POP`;
4. **interrupts** when the queue fill passes `CTRL.BUFFER_THRESHOLD` (level), or
   when a message arrives with no `last_packet` marker (`MISSING_LAST`, sticky).

It can also ask the transmitter to flush in-flight telemetry
(`CTRL.TELEMETRY_TX_FLUSH` → `afvalid_o` / `afready_i`), and discard everything
it holds (`CTRL.TELEMETRY_RX_FLUSH`).

---

## Layout

```
telemetry_receiver/
├── CMakeLists.txt
├── README.md                        (this file)
├── run_tests.sh                     Build + run convenience script
├── deps.env.example                 Template for local dependency paths
├── doc/
│   ├── 01_TELEMETRY_RECEIVER_Specification.md
│   ├── 02_TELEMETRY_RECEIVER_LowLevel_Design.md
│   └── 03_TELEMETRY_RECEIVER_Test_Plan.md
├── include/
│   └── telemetry_receiver.h         SC_MODULE(telemetry_receiver) declaration,
│                                    ATB packet accessors + reference encoder
│                                    (uses shared smc/common/include/smc_axi_extension.h)
├── src/
│   └── telemetry_receiver.cpp       Implementation
└── test/
    ├── CMakeLists.txt
    ├── telemetry_receiver_tb.cpp     Primary self-checking test bench
    └── telemetry_receiver_neg_tb.cpp Negative-path / edge-case test bench
```

---

## Building and testing

First-time setup (point the build at your SystemC + CCI installs):

```bash
cp deps.env.example deps.env
# edit SYSTEMC_HOME and CCI_HOME in deps.env
./run_tests.sh
```

`run_tests.sh` options:

| Flag | Effect |
|------|--------|
| (none) | Incremental Release build + run |
| `--clean` | Wipe `build/` first |
| `--ctest` | Run via `ctest` |
| `--asan` | Build with AddressSanitizer (Linux: + LeakSanitizer) |
| `--coverage` | Build with coverage; print a line report |

Both test benches print `ALL TESTS PASSED` on success. Line coverage of
`src/telemetry_receiver.cpp` is 99 % (the remainder is two `SC_REPORT_FATAL`
config guards and a gcov artefact — see the test plan §7).

> **Local ASan note.** On RHEL 8 the gcc-toolset ASan runtime is not packaged
> (`cannot find -lasan`), and a SystemC built with the default QuickThreads
> coroutines is not ASan-compatible at all. For a local ASan run use
> `clang++` together with a SystemC configured `-DENABLE_PTHREADS=ON`. See
> `doc/03_TELEMETRY_RECEIVER_Test_Plan.md` §8.

---

## Address map

Each instance occupies a `0x100` window; the SMC wrapper packs three of them
(`+0x000`, `+0x100`, `+0x200`).

> **Base address (RTL-aligned):** the wrap sits at **`0xC000_9000`** in
> `smc-vp` (`smc_top.rdl`). I2C lives at `0xC000_5000`; I3C at `0xC003_A000`.
> This model decodes **window-relative offsets only**.

All registers are 32-bit, accessed with 4-byte, 4-byte-aligned transactions.

| Offset | Name | SW | Notes |
|--------|------|----|-------|
| `0x00` | `CTRL` | rw | `[0]` `BUFFER_POP` (write pulse), `[4]` `TELEMETRY_RX_FLUSH` (write pulse), `[8]` `TELEMETRY_TX_FLUSH` (HW-cleared on `afready_i`), `[23:12]` `BUFFER_THRESHOLD` |
| `0x04` | `STATUS` | ro | `[0]` `BUFFER_EMPTY` (set at reset), `[4]` `BUFFER_FULL` |
| `0x08` | `INTR_STATUS` | rw | `[0]` `MISSING_LAST` (sticky, **W1C**), `[4]` `BUFFER_THRESHOLD` (read-only level mirror) |
| `0x0C` | `INTR_ENABLE` | rw | `[0]` `MISSING_LAST`, `[4]` `BUFFER_THRESHOLD` |
| `0x10` | `INTR_TEST` | rw | `[0]` `MISSING_LAST` (write pulse), `[4]` `BUFFER_THRESHOLD` (stored level) |
| `0x14` | `TELEMETRY_PROBE_ID` | ro | `[4:0]` probe ID of the oldest queued message |
| `0x18` | `TELEMETRY_COUNTER_VLDS` | ro | per-counter valid bits of the oldest queued message |
| `0x80` + 4·*i* | `TELEMETRY_COUNTER[i]`, *i* = 0..31 | ro | counter *i* of the oldest queued message |

The `[0x1C, 0x80)` gap, offsets `>= 0x100`, and misaligned accesses return
`TLM_ADDRESS_ERROR_RESPONSE`. `TELEMETRY_COUNTER[i]` for
`i >= max_counters_per_message` reads 0 (tied off in the RTL). While the queue
is empty the whole message view reads 0.

> **Firmware note.** `CTRL` is one register: pulse `BUFFER_POP` / `RX_FLUSH` with
> a read-modify-write, or the write will also zero `BUFFER_THRESHOLD` (typically
> leaving the threshold interrupt asserted).

---

## Configuration (CCI)

| CCI parameter | Type | Default | Mutability | Notes |
|---------------|------|---------|------------|-------|
| `buffer_depth` | `cci_param<unsigned, CCI_IMMUTABLE_PARAM>` | 8 | immutable | Message-queue depth (≥ 2); also sets the threshold compare width |
| `max_counters_per_message` | `cci_param<unsigned, CCI_IMMUTABLE_PARAM>` | 4 | immutable | Counters per message (1..32); sizes the assembly buffer |
| `access_delay_ns` | `cci_param<double>` | 2.0 | mutable | Annotated TLM access latency (AXI4-Lite) |

Set presets via the broker before construction:

```cpp
broker.set_preset_cci_value("top.telemetry0.buffer_depth", cci::cci_value(16u));
broker.set_preset_cci_value("top.telemetry0.max_counters_per_message",
                            cci::cci_value(8u));
```

At construction the model logs the resolved values, whether each came from a
preset (`[preset]`) or the default (`[default]`), the derived assembly-buffer
size, and the threshold wrap mask.

---

## Ports

| Port | Dir | Description |
|------|-----|-------------|
| `reg_socket` | target | TLM-2.0 LT register socket (AXI4-Lite-style, 32-bit) |
| `rst_n_i` | in | Active-low asynchronous reset |
| `afready_i` | in | Transmitter flush acknowledge (ATB AF channel) |
| `irq_o` | out | Interrupt: `MISSING_LAST \|\| BUFFER_THRESHOLD` |
| `afvalid_o` | out | Transmitter flush request (`CTRL.TELEMETRY_TX_FLUSH`) |
| `atready_o` | out | ATB ready (de-asserted only while in reset) |
| `debug_o` | out | 4-bit debug vector: `[0]` missing-last, `[1]` buffer full, `[2]` buffer empty, `[3]` assembly full |

`recompute_method` is the sole driver of all four outputs (single-driver
discipline).

---

## ATB message format (summary)

Eight byte beats form a 64-bit packet; packets form a message:

```
 bit 63          62 61....54  53 52....45        8 7.....0
    +-----------+---+--------+---+--------+ ... +---+------+
    |last_packet|vld|  data  |vld|  data  |     |vld| data |
    +-----------+---+--------+---+--------+     +---+------+
                 \__block 0_/ \__block 1_/       \_block 6_/
```

- Beat *i* carries packet bits `[8i+7 : 8i]` — the first beat is the packet LSB,
  the last carries `last_packet`.
- `blocks[0]` of the first packet is the header; the probe ID is packet bits
  `[60:56]`.
- Remaining blocks are counter payload, **MSB byte first**, 4 blocks per
  counter, running across packet boundaries. A counter is valid only if all four
  blocks are valid.
- A message spans `ceil((1 + 4 * max_counters_per_message) / 7)` packets.

Full description, including a worked byte-level example, is in
`doc/01_TELEMETRY_RECEIVER_Specification.md` §4.

---

## Test-bench back door

Because there is no ATB initiator in the model, beats are injected through a
back door instead of a port (mirroring how `beu` abstracts its error sources and
`i2c_controller` its bus):

- `push_atb_beat(beat)` / `push_atb_beats(beats)` — inject ATB traffic; return
  false / a short count if the receiver is not accepting (in reset)
- `telemetry_encode_message(probe_id, counters, max_counters, set_last_packet)` —
  reference encoder (the inverse of the model's decoder) for building conforming
  beat streams; pass `set_last_packet = false` to provoke a missing-last event
- `fill_level()` — current message-queue occupancy
- `dbg_reg(off)` — side-effect-free register peek
- `dump_state(os)` — human-readable state dump

See `doc/02_TELEMETRY_RECEIVER_LowLevel_Design.md` for details.
