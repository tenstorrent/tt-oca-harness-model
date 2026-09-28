# SMC `telemetry_receiver` test audit and remediation plan

## Audit scope

Audited:

- `smc/peripherals/telemetry_receiver/include/telemetry_receiver.h`
- `smc/peripherals/telemetry_receiver/src/telemetry_receiver.cpp`
- `smc/peripherals/telemetry_receiver/test/telemetry_receiver_tb.cpp`
- `smc/peripherals/telemetry_receiver/test/telemetry_receiver_neg_tb.cpp`
- `smc/peripherals/telemetry_receiver/test/CMakeLists.txt`
- `smc/peripherals/telemetry_receiver/CMakeLists.txt`
- `smc/peripherals/telemetry_receiver/run_tests.sh`

No model or test was modified.

## Model behavior inventory

- Ports: 32-bit MMIO target, reset, AF flush acknowledge, IRQ, AF flush request, ATB ready, and debug vector.
- Data ingress: public `push_atb_beat(s)` backdoor; no ATB data/valid input ports.
- Message encoding/decoding: 8 byte-beats per 64-bit packet, seven `{valid,data}` blocks, first-block probe ID, four MSB-first blocks per counter, configurable 1..32 counters.
- Queue: configurable circular FIFO, oldest visible, pop, full/empty status, drop-oldest overflow.
- Registers: `CTRL`, `STATUS`, `INTR_STATUS/ENABLE/TEST`, visible probe ID, valid bitmap, 32 counter words; in-window gaps fail decode.
- Interrupts: gated sticky missing-last with W1C; enabled level threshold/test interrupt; priority is aggregate OR at `irq_o`.
- Flush/debug: RX flush drops assembly/queue and clears debug latches; TX flush holds AF valid until ready; debug bits latch selected events.
- Processes/events: reset, AF handshake, zero-time recompute, start-of-simulation initial drive, AF retry event.
- TLM: 32-bit aligned access and mutable delay; debug reads use side-effect-free `dbg_reg`, debug writes invoke normal side effects.

## Existing test classification

| Existing test area | Classification | Rationale |
|---|---|---|
| Register reset/RW/RO/masks (`telemetry_receiver_tb.cpp:201-251`) | Genuine | Frontdoor and explicit values. |
| One-counter hand-derived packet (`telemetry_receiver_tb.cpp:253-286`) | Genuine | Independent fixed packet oracle validates one packet. |
| Multi-packet/max geometry (`telemetry_receiver_tb.cpp:288-304`; negative `311-340`) | Partial | Generated and decoded by shared encoder/packing helpers, so most expectations are implementation-coupled. |
| FIFO/pop/overflow (`telemetry_receiver_tb.cpp:327-358`) | Partial | Functional, but ingress is exclusively a test backdoor. |
| Interrupt and flush behavior (`telemetry_receiver_tb.cpp:360-529`) | Partial/Genuine | Strong observable assertions after backdoor events; exact temporal boundaries and malformed packets are absent. |
| Debug peeks/dump (`telemetry_receiver_tb.cpp:532-579`; negative `219-249, 362`) | Coverage-only | Direct internals and text output add little frontdoor assurance. |
| Negative TLM matrix (`telemetry_receiver_neg_tb.cpp:164-215`) | Genuine/Partial | Good command/size/address/decode checks, but no BE/streaming/null/DMI coverage. |
| Extreme CCI geometry (`telemetry_receiver_neg_tb.cpp:289-360`) | Partial | Valuable bounds exercise but circular encoder/decoder oracle. |

No private-public macro or test-only model branch was found.

## Artificial coverage mechanisms

- All telemetry data enters through `push_atb_beat(s)` (`include/telemetry_receiver.h:372-389`), not a signal/TLM frontdoor.
- Most messages are created by `telemetry_encode_message`, the inverse implemented beside the decoder (`src/telemetry_receiver.cpp:36-81`); only one one-counter packet is independently derived.
- Public internals `fill_level`, `dbg_reg`, and `dump_state` are used repeatedly.
- The negative CMake target is explicitly justified as bringing line coverage above 95% (`test/CMakeLists.txt:20-26`).
- Logic-X reports are globally suppressed in both benches (`test/telemetry_receiver_tb.cpp:589-590`, `test/telemetry_receiver_neg_tb.cpp:374-375`).

## Findings

1. **High — the telemetry datapath has no production frontdoor.** The model documents ATB but exposes only `atready_o`; data and valid are injected through a test-bench API (`include/telemetry_receiver.h:330-389`). Thus queue/decode tests do not prove platform wiring or handshake behavior.
2. **High — early `last_packet` can decode stale assembly bytes.** On `last_packet`, `decode_message` always reads the configured full message geometry, then only resets `beats_` (`src/telemetry_receiver.cpp:292-317, 365-383`). `rx_flush` also does not clear `assembly_` (`src/telemetry_receiver.cpp:347-354`). A short message after an earlier long/partial message can inherit stale counter blocks.
3. **High — MMIO lacks null, streaming-width, and byte-enable validation.** A legal command/length/address immediately copies through `data_ptr`; BE and streaming fields are ignored (`src/telemetry_receiver.cpp:517-565`).
4. **Medium — most decoder tests share the encoder implementation.** The encoder and decoder use the same packet/block helpers and geometry (`src/telemetry_receiver.cpp:36-81, 292-317`), making multi-packet and 32-counter tests vulnerable to matched errors.
5. **Medium — debug transport lacks pointer/BE/streaming validation.** Debug writes invoke normal side effects, while malformed protocol fields are not checked (`src/telemetry_receiver.cpp:567-590`).
6. **Medium — constructor timing validation is absent.** `access_delay_ns` is mutable and used directly to construct `sc_time` per request, but negative values are not rejected (`src/telemetry_receiver.cpp:88-126, 560`).
7. **Medium — temporal/interrupt races are not covered.** Missing-last coincident with enable changes, W1C coincident with a new event, AF ready toggling in the same delta as TX flush, reset during partial assembly, and threshold crossing on overflow/pop need delta-precise assertions.
8. **Closed (2026-09-24) — negative-bench process failures are no longer hidden in ASan mode.** Combined child exits fail the ASan phase. SMC orchestrator ASan PASS.
9. **Low — AXI extension and DMI are not behaviorally tested.** The extension is fetched then ignored (`src/telemetry_receiver.cpp:541-544`); successful requests set DMI false, but no DMI query exists.
10. **Low — CCI mutation is checked only at the handle.** The test changes delay 3→5 ns without measuring an annotated transaction (`test/telemetry_receiver_tb.cpp:552-568`).

## Missing scenarios

- Signal/TLM frontdoor ATB valid/ready data transfer, backpressure, and reset behavior.
- Early last after one packet for a multi-packet geometry, especially after prior nonzero/partial data.
- `last_packet` at each legal packet boundary; malformed/invalid header block; mixed valid bits within one counter using independent raw beats.
- Multiple messages concatenated without idle; packet split across reset/RX flush.
- Overflow and threshold changes in the same delta; pop+flush+TX flush combinations.
- Missing-last event with enable/W1C races and exact sticky semantics.
- AF ready before, with, and after TX flush; reset while AF valid.
- Full MMIO protocol matrix: null, BE, streaming width, delay, DMI, extension.
- Invalid CCI depth/counter/delay with exact fatal expectations.

## Proposed testcases

| ID | Stimulus | Expected result | Path exercised | Feature proved |
|---|---|---|---|---|
| TEL-ATB-001 | Drive a real ATB data/valid frontdoor while honoring ready. | Exactly one beat accepted per handshake; reset/backpressure reject beats. | production ingress | Platform connectivity |
| TEL-DEC-001 | Inject fixed independently generated raw bytes for 1-, 2-, 3-, and 19-packet messages. | Probe/counters/valid bits match external oracle. | packet/decode helpers | Decoder correctness |
| TEL-DEC-002 | End a configured long message after packet 1, following a nonzero long message and partial RX flush. | Unreceived counters are invalid/zero; no stale bytes leak. | early-last/assembly reuse | Assembly isolation |
| TEL-DEC-003 | Make one of four blocks invalid for each byte position of a counter. | Counter valid clears and value reads zero in every case. | `decode_message` | Valid reduction |
| TEL-FIFO-001 | Fill, overflow twice, pop through wraparound, then refill. | Strict oldest-visible/drop-oldest ordering at every step. | circular queue | Pointer wrap |
| TEL-IRQ-001 | Cross threshold via push/pop/overflow and toggle enable/test concurrently. | Level status and IRQ follow an independently computed count/threshold oracle. | threshold logic | Interrupt level semantics |
| TEL-IRQ-002 | Generate missing-last in same delta as enable/W1C changes. | Defined gate/sticky precedence; no lost or phantom event. | sticky status | Interrupt race semantics |
| TEL-FLUSH-001 | Assert TX flush with AF ready before/same/after write; reset while pending. | AF valid lifecycle and CTRL self-clear are delta-accurate. | AF process/event | Flush handshake |
| TEL-RST-001 | Reset and RX flush at each packet boundary and mid-packet. | Assembly/queue/debug/interrupt state is completely cleared. | `reset_proc`, `rx_flush` | Recovery |
| TEL-TLM-001 | MMIO command/length/alignment/range/null/BE/streaming matrix. | Exact errors; rejected requests cause no register/FIFO side effects. | `b_transport` | Protocol safety |
| TEL-DBG-001 | Debug read/write/ignore with malformed fields. | Exact byte counts and explicitly documented write side effects. | `transport_dbg` | Debug contract |
| TEL-CCI-001 | Change access delay and measure consecutive transactions; construct invalid geometries/delay. | Runtime delay follows CCI; invalid configs fail narrowly. | CCI/constructor | Configuration |
| TEL-AXI-001 | Attach canonical AXI extension and query DMI. | Sideband accepted without corruption; DMI explicitly denied. | TLM integration | AXI/DMI |

## Verdict

**High integration risk despite broad line coverage.** Register, queue, interrupt, and flush logic have many assertions, but the core telemetry path is exercised only through a backdoor and mostly with a co-implemented encoder. The ASan negative-bench exit hole is **closed (2026-09-24)**. The stale-byte early-last case and incomplete MMIO validation remain concrete high-priority gaps.
