# SMC Telemetry Receiver — Test Plan

> Verification strategy for the SystemC/TLM-2.0 LT model in
> `smc/peripherals/telemetry_receiver`. Benches:
> `test/telemetry_receiver_tb.cpp` (primary) and
> `test/telemetry_receiver_neg_tb.cpp` (negative / edge). Both self-check and
> print `ALL TESTS PASSED`.

## 1. Objectives

- Confirm the register map (offsets, access types, masks, reset values) matches
  `telemetry_receiver.rdl`.
- Confirm the ATB decode: beat ordering, packet packing, probe-ID placement,
  MSB-first counter bytes, per-counter validity, and messages spanning multiple
  packets — **validated against a hand-derived beat stream**, not against the
  model's own encoder.
- Confirm message-buffer semantics: FIFO order, `BUFFER_POP`, `STATUS`
  empty/full, and the RTL's **drop-oldest** overflow policy.
- Confirm both interrupt sources, including their different flavours (level
  mirror vs. sticky W1C), enable gating at event time, `INTR_TEST` forcing, and
  threshold truncation.
- Confirm flush behaviour (RX flush of queue + partial message, TX flush
  handshake in both `afready_i` orders) and reset behaviour.
- Confirm robust bus behaviour: TLM response codes for malformed accesses.
- Achieve ≥ 95 % line coverage of `src/telemetry_receiver.cpp`
  (achieved: **99 %**, see §7).

## 2. Environment

- Accellera SystemC 2.3.x / 3.0.x + TLM-2.0, SystemC CCI 1.0, C++17/20.
- A tiny TLM initiator (`driver`) with a `tlm_quantumkeeper` issues 32-bit
  reads/writes and `transport_dbg` peeks. In the negative bench a `raw_driver`
  returns the `tlm_response_status` instead of failing the test, so error paths
  can be asserted on.
- The DUT outputs (`irq_o`, `afvalid_o`, `atready_o`, `debug_o`) are wired to
  `sc_signal`s observed by the bench; `rst_n_i` and `afready_i` are bench-driven.
- ATB traffic is injected through the `push_atb_beat()` / `push_atb_beats()`
  back door; streams are built with `telemetry_encode_message()` except where an
  independent oracle is required (§4, test 3).

### 2.1 Multiple DUT geometries

Because the assembly buffer and message queue are sized by **immutable** CCI
parameters, geometry coverage needs several DUT instances rather than run-time
reconfiguration. Four are instantiated across the two benches:

| Bench | Instance | `buffer_depth` | `max_counters_per_message` | Packets / message | Purpose |
|-------|----------|---------------|---------------------------|-------------------|---------|
| primary | `tb.tel`   | 4 | 4 (default) | 3 | Multi-packet messages, queue behaviour, interrupts |
| primary | `tb.tel1`  | 2 | 1 | 1 | Single-packet message → hand-derivable beat oracle |
| negative | `tb.telmin` | 2 (minimum) | 1 | 1 | Error paths, empty-pop, threshold truncation |
| negative | `tb.telmax` | 2 | 32 (maximum) | 19 | Widest message; counter array boundary |

## 3. Test-bench architecture

| Bench | Focus |
|-------|-------|
| `telemetry_receiver_tb` | Functional happy path: reset, register RW/RO/masking, ATB decode, queue, interrupts, flushes, reset recovery, `transport_dbg`, CCI. |
| `telemetry_receiver_neg_tb` | Error / edge branches: TLM response codes, decode miss, `transport_dbg` edges, RO write-ignore, empty pop, flush precedence, threshold truncation, extreme geometries. |

## 4. Primary test list (`telemetry_receiver_tb`)

| # | Test | Checks |
|---|------|--------|
| 1 | Reset values | `CTRL=0`, `STATUS=BUFFER_EMPTY`, `INTR_*=0`, `PROBE_ID=0`, `COUNTER_VLDS=0`, `COUNTER[0]=0`; `irq_o` low, `afvalid_o` low, `atready_o` **high**, `debug_o=BUFFER_EMPTY`, `fill_level()=0`. |
| 2 | Register RW / RO / masking | `CTRL.BUFFER_THRESHOLD` stores all 12 bits; the `BUFFER_POP` / `RX_FLUSH` pulse bits read back 0; `INTR_ENABLE` masks reserved bits to `0x11`; `INTR_TEST` keeps only the level bit; writes to `STATUS`, `PROBE_ID`, `COUNTER_VLDS`, `COUNTER[i]` are ignored. |
| 3 | **Hand-derived beat stream** | A beat stream derived by hand from `telemetry_receiver_pkg.sv` (probe `0x15`, `counter[0]=0xDEADBEEF`, packet word `0xD53BDADDF7BC0000`) decodes to the expected probe ID, `VLDS=0x1`, and counter value — and `telemetry_encode_message()` reproduces that same stream byte-for-byte. This is the one test that pins the wire format independently of the model. |
| 4 | Multi-packet message | 4 counters = 3 packets = 24 beats; all four counter values, `VLDS=0xF`, probe ID; `COUNTER[4]` and `COUNTER[31]` read 0 (tied off above `max_counters_per_message`). |
| 5 | Per-counter valid bits | Counters 1 and 3 encoded invalid → `VLDS=0x5` and those registers read 0, while 0 and 2 carry their values; full 5-bit probe ID (`0x1F`). |
| 6 | FIFO order / `BUFFER_POP` / `STATUS` | 4 messages fill the queue → `STATUS=BUFFER_FULL`, `debug_o=BUFFER_FULL`; popping yields them **oldest-first**; after draining, `STATUS=BUFFER_EMPTY` and the message view reads 0. |
| 7 | Drop-oldest overflow | 6 messages into a 4-deep queue → `fill_level()=4` and the visible message is the **3rd** (first two dropped), proving the newest telemetry survives. |
| 8 | `BUFFER_THRESHOLD` interrupt | Threshold 1: no IRQ at fill 1, IRQ at fill 2; `INTR_STATUS` mirrors it; a W1C write does **not** clear a level source; a bare `CTRL=BUFFER_POP` write clobbers the threshold to 0 and keeps the IRQ asserted, while a threshold-preserving pop clears it; clearing `INTR_ENABLE` masks the line while still over threshold. |
| 9 | `INTR_TEST.BUFFER_THRESHOLD` | Forces the IRQ with an empty queue; writing 0 releases it. |
| 10 | Missing-last event + recovery | A message with no `last_packet` is discarded (`fill_level()=0`), raises the IRQ, sets `INTR_STATUS.MISSING_LAST` and `debug_o` bits 0 and 3; W1C clears status and IRQ; the next well-formed message then decodes normally. |
| 11 | Missing-last enable gating | With `INTR_ENABLE=0` the event sets **no** status and no IRQ, but `debug_o[0]` still records it. |
| 12 | `INTR_TEST.MISSING_LAST` | Ignored while the enable is clear; with the enable set it latches the sticky status and raises the IRQ; the test bit itself never reads back (pulse); W1C clears. |
| 13 | RX flush | Flushes 2 queued messages **and** a one-packet partial assembly; `STATUS=BUFFER_EMPTY` afterwards and the next message decodes cleanly (proving the stale beats are gone, not merely hidden). |
| 14 | TX flush handshake | With `afready_i` low: `afvalid_o` asserts and `CTRL.TX_FLUSH` reads 1; asserting `afready_i` retires the request and self-clears the bit. With `afready_i` already high, the flush retires in the same write. |
| 15 | Reset clears state / refuses beats | Reset with a queued message and `INTR_ENABLE` set: `atready_o` de-asserts, `push_atb_beat()` and `push_atb_beats()` are refused, and after release the queue and registers are back to reset values. |
| 16 | `transport_dbg` / `dbg_reg` peek | Debug reads return 4 and the right values without disturbing the queue; `dbg_reg()` on an unmapped offset returns 0. |
| 17 | CCI introspection | `buffer_depth` / `max_counters_per_message` presets visible per instance and flagged `is_preset_value()`; `access_delay_ns` preset readable and mutable through a broker handle; full parameter list dumped. |

## 5. Negative test list (`telemetry_receiver_neg_tb`)

| # | Test | Expected |
|---|------|----------|
| 1 | Ignore command | `TLM_COMMAND_ERROR_RESPONSE` |
| 2 | Wrong width (8, 2, 1 bytes) | `TLM_BURST_ERROR_RESPONSE` |
| 3 | Out-of-window (`0x100`, `0x1000`) and misaligned (`0x02`, `0x0A`) | `TLM_ADDRESS_ERROR_RESPONSE` |
| 4 | In-window decode miss (`0x1C`, `0x20`, `0x40`, `0x7C`; read + write) | `TLM_ADDRESS_ERROR_RESPONSE`, while `COUNTER[0]` and `COUNTER[31]` return `TLM_OK_RESPONSE` |
| 5 | `transport_dbg` rejects | bad length / misaligned / out-of-window / ignore command / write to unmapped offset → returns 0 |
| 6 | `transport_dbg` accepts | write + read back of `INTR_ENABLE` → returns 4, value updates; write to RO `COUNTER[1]` and `STATUS` returns 4 but leaves the value at 0 |
| 7 | `dbg_reg` guards | `dbg_reg(0x100)` (past the counter array) and `dbg_reg(0x1C)` return 0 instead of indexing out of range |
| 8 | `BUFFER_POP` on an empty queue | Ignored three times over — no pointer underflow; `STATUS` stays `BUFFER_EMPTY` and the queue still works afterwards |
| 9 | Flush precedence | A single `CTRL` write setting both `RX_FLUSH` and `BUFFER_POP` discards the **whole** queue, not one entry |
| 10 | Threshold truncation | With `buffer_depth=2` (compare width 2 bits) a programmed threshold of `0x5` behaves as `1`: no IRQ at fill 1, IRQ at fill 2 |
| 11 | Maximum geometry (32 counters) | `telemetry_packets_per_message(32) == 19`; a 152-beat message is fully accepted; all 32 counter values decode, `COUNTER_VLDS = 0xFFFFFFFF` |
| 12 | Missing-last on the maximum geometry | A 19-packet message with no marker raises the sticky IRQ; W1C clears it |

## 6. Running

```bash
./run_tests.sh              # Release build + run both benches
./run_tests.sh --ctest      # via ctest
./run_tests.sh --asan       # AddressSanitizer + LeakSanitizer  (separate build)
./run_tests.sh --coverage   # coverage + line report            (separate build)
```

`--asan` and `--coverage` are mutually exclusive and each use an isolated build
directory (`build_asan/`, `build_cov/`), per
`.cursor/rules/test-coverage-asan.mdc`.

## 7. Coverage

`./run_tests.sh --coverage` reports:

| File | Lines | Covered | % |
|------|-------|---------|---|
| `include/telemetry_receiver.h` | 21 | 21 | **100 %** |
| `src/telemetry_receiver.cpp` | 355 | 352 | **99 %** |

The three uncovered lines in `src/telemetry_receiver.cpp` are:

- the two `SC_REPORT_FATAL` bodies for the constructor's CCI range checks
  (`buffer_depth >= 2`, `max_counters_per_message` in 1..32). Reaching them
  aborts elaboration, which cannot be done in-process without leaving the
  SystemC module hierarchy in an inconsistent state — these are exactly the
  "defensive guards behind `SC_REPORT_FATAL` invariants" the coverage rule
  exempts;
- the closing brace of `decode_message()`, where gcov attributes the
  exception-unwind path of returning a `telemetry_message` by value (an
  artefact, not a reachable branch).

Both benches contribute: the primary bench covers the functional paths, the
negative bench covers the four TLM error branches, the in-window decode miss,
the `transport_dbg` rejects, the empty-pop guard, and the two extreme
geometries.

## 8. AddressSanitizer / UBSan

`./run_tests.sh --asan` produces **no ASan or UBSan errors and no leaks
attributable to the model** — no frame in any report points into
`src/telemetry_receiver.cpp`.

Two environment notes for anyone reproducing this locally:

1. **RHEL 8 / gcc-toolset cannot link ASan** (`cannot find -lasan`): the ASan
   runtime is not packaged for gcc-toolset-12/13, which is also why
   `.github/workflows/ci-rhel8.yml` skips the ASAN phase. Use the Ubuntu CI job,
   or a local `clang++` (`CXX=/usr/bin/clang++ ./run_tests.sh --asan`).
2. **A QuickThreads SystemC build is not ASan-compatible.** SystemC's default
   coroutine package switches stacks in assembly, which ASan cannot follow: the
   first `sc_signal::write()` inside any `SC_THREAD` faults with a spurious
   `SEGV` in `qt_abort`. This is not model-specific — the `beu` bench fails
   identically. Build SystemC with `-DENABLE_PTHREADS=ON` (or
   `--enable-pthreads`) for local ASan runs.

The only leak reports that remain come from the global CCI broker that every
peripheral bench in this repo creates and intentionally never deletes
(`cci_register_broker(new cci_utils::consuming_broker(...))`) plus allocations
inside `libcci-config.so` itself. `beu` reports the same pattern from the same
line of its own `sc_main`.

## 9. Pass / fail criteria

- Each bench increments a failure counter on any failed `EXPECT_*` and prints
  `ALL TESTS PASSED` iff the counter is zero (exit code 0).
- CTest matches `ALL TESTS PASSED` / `FAILURE(S)` via the regexes in
  `test/CMakeLists.txt`.
- CI regression is driven by `smc/run_all_smc_tests.sh telemetry_receiver`
  (Release, ASAN, Coverage, CTest stages) and by the `smc-unit-tests` matrix in
  `.github/workflows/ci.yml` / `ci-rhel8.yml`.

## 10. Not covered by these benches

- **Platform integration.** Wiring the receiver into `smc-vp` (address decode at
  the yet-to-be-agreed wrapper base, PLIC interrupt line, three instances) and a
  firmware-level drain test are out of scope here; they follow the pattern of
  `beu/doc/04_BEU_Platform_Integration_Test_Plan.md` and depend on resolving the
  address-map conflict in `01_TELEMETRY_RECEIVER_Specification.md` §2.1.
- **ATB protocol timing / back-pressure**, since there is no ATB port
  (`atready_o` reflects reset only).
- **Transmitter behaviour.** The TX flush handshake is checked from the
  receiver's side only; no transmitter model exists.
