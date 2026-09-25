# SMC `uart` test audit and remediation plan

## Audit scope

Audited:

- `smc/peripherals/uart/include/uart.h`
- `smc/peripherals/uart/src/uart.cpp`
- `smc/peripherals/uart/test/uart_tb.cpp`
- `smc/peripherals/uart/test/CMakeLists.txt`
- `smc/peripherals/uart/CMakeLists.txt`
- `smc/peripherals/uart/run_tests.sh`

This includes both `uart` and `uart_wrap`/log-engine CSRs. No model or test was modified.

## Model behavior inventory

- UART ports: MMIO target; reset; serial TX/RX; four active-low modem inputs/outputs; RX/TX DMA ready; aggregate error; IRQ.
- UART registers: DLAB-banked RBR/THR/DLL and IER/DLM; IIR/FCR; LCR; MCR; LSR; MSR; SCR; ECR; ITR.
- Datapath: character-granularity RX injection; TX history pop; FIFO/non-FIFO queues; divisor gating; immediate LT TX drain; system and line loopback; break output.
- Status/side effects: RBR pop/timeout clear, IIR THRE clear, LSR sticky error clear, MSR delta clear, FCR FIFO reset/self-clear, FIFO masks.
- Interrupts: FIFO error, line status, timeout, data ready, THRE, modem status, plus ITR force bits and documented priority.
- DMA: mode 0 ready levels and mode 1 RX/TX state.
- Processes/events: reset, modem edge detection, zero-time output recomputation, 1 us coarse RX timeout.
- TLM: 32-bit MMIO, DLAB-aware decode, mutable delay, debug transport, DMI flag false on successful normal access.
- Wrapper: UART enable, log enable/config/address, interrupt status/enable/test, and 16 log-entry registers; in-window holes RAZ/WI; disabling log engine resets its CSRs.

## Existing test classification

| Existing test area | Classification | Rationale |
|---|---|---|
| Register/DLAB/reset/masks (`uart_tb.cpp:240-269, 640-669`) | Genuine plus coverage-only tail | Main checks are frontdoor; section 25 is explicitly decode/debug coverage. |
| TX/RX/FIFO (`uart_tb.cpp:271-364`) | Partial | MMIO is frontdoor, but characters enter/leave only through `inject_rx_char` and `dbg_tx_pop`. |
| Interrupts/errors/modem (`uart_tb.cpp:286-442, 548-581`) | Genuine/Partial | Observable IRQ/IIR/ports are checked; simultaneous priority and persistent-error cases are missing. |
| Loopback/break/modem outputs (`uart_tb.cpp:444-505`) | Genuine | Uses public inputs/outputs and MMIO controls. |
| DMA (`uart_tb.cpp:507-545, 671-687`) | Partial | Public outputs checked, but instant TX drain and debug injection avoid realistic flow control. |
| TLM errors (`uart_tb.cpp:601-619`) | Genuine/Partial | Covers command/length/address/decode only. |
| Debug and dump (`uart_tb.cpp:621-669, 751-752`) | Coverage-only | Direct peeks/history/text formatting. |
| Wrapper CSRs (`uart_tb.cpp:720-749`) | Partial | Storage/masks/W1C are checked; no actual logging or interrupt output behavior exists. |

No private-public macro, touch-all-offset loop, or test-only source branch was found.

## Artificial coverage mechanisms

- RX and TX functional checking depends on public test backdoors (`inject_rx_char`, `dbg_tx_pop`, count APIs).
- `uart_tb.cpp:640-669` explicitly performs remaining-register and debug coverage.
- `dbg_reg`, debug transport writes, and `dump_state` exercise implementation accessors.
- Logic-X conversion reports are suppressed globally (`test/uart_tb.cpp:763-764`).

## Findings

1. **High — serial functionality is not frontdoor-tested or modeled at character transfer level.** `rx_i` is sampled only in line-loopback output logic, and normal TX remains idle high except break (`src/uart.cpp:645-672`). All normal characters use `inject_rx_char`/`dbg_tx_pop` (`src/uart.cpp:790-812`). This is acceptable only if the platform explicitly requires a character API and wires it; current tests do not prove that integration.
2. **High — RX timeout is not rearmed when RBR is read but data remains.** `rbr_read` clears pending status but neither cancels nor rearms the existing event (`src/uart.cpp:332-344`), while timeout is armed only on delivery (`src/uart.cpp:284-287, 310-325`). Multi-character reads can therefore time out from the previous arrival rather than the most recent bus activity.
3. **High — UART and wrapper MMIO omit null, byte-enable, and streaming-width checks.** Both targets copy through `data_ptr` after command/length/address checks (`src/uart.cpp:717-763, 961-994`). Malformed legal-sized requests can crash or be silently accepted.
4. **High — `uart_wrap` is only a CSR shell.** Log region/write address/entry registers have no logging datapath, and interrupt enable/status has no output port or integration behavior (`include/uart.h:590-649`, `src/uart.cpp:889-1014`). The existing test proves storage only.
5. **Medium — interrupt priority is not tested with simultaneous real sources.** Individual and two ITR cases are covered, but the full six-level arbitration and source-clearing transition order are not.
6. **Medium — FIFO boundary/drop semantics are incomplete.** RX FIFO full/drop with configured depth, TX full/drop, FIFO disable flush, trigger selectors above index 1, and depth-1/4096 CCI limits are absent.
7. **Medium — persistent RX error behavior is weakly asserted.** In FIFO mode, LSR read clears front status bits while `any_rx_error` can keep ERRF/`err_o` and FIFO-error IRQ active until RBR pop (`src/uart.cpp:366-378, 580-609, 697-710`). The suite does not verify the full sequence.
8. **Medium — debug transport shares write side effects and lacks protocol validation.** Only a read side-effect check and one SCR write are covered (`test/uart_tb.cpp:621-669`).
9. **Medium — access-delay mutation is not measured.** The CCI handle changes to 7 ns, but no annotated delay assertion follows (`test/uart_tb.cpp:691-717`).
10. **Low — no constructor guard tests exist for FIFO depths 0 and 4097.** Fatal branches at `src/uart.cpp:125-128` are uncovered functionally.
11. **Low — AXI sideband is fetched but ignored, and DMI is not queried.** `src/uart.cpp:738-741`.

## Missing scenarios

- Platform-facing character transport or serial adapter behavior for ordinary RX/TX, including backpressure.
- RX timeout with multiple queued characters and RBR reads just before/at timeout.
- Full interrupt-priority chain with simultaneous conditions and stepwise clearing.
- RX/TX exact capacity, extra push/write drop policy, FIFO enable toggles, reset bits, and every trigger selector/clamp.
- Error flags on front, middle, and last RX FIFO entries; ERRF/line IRQ/error output after LSR and RBR sequences.
- THRE transitions on divisor programming, FCR TX reset, full FIFO, and IIR reads.
- Modem all inputs/deltas, loopback precedence, RI non-trailing edge, and reset rebaseline.
- MMIO null/BE/streaming/delay/DMI/AXI-extension matrix for both targets.
- Wrapper reset, all 16 entries, hole policy boundaries, and actual log/interrupt behavior.
- Invalid CCI bounds and immutable mutation attempts.

## Proposed testcases

| ID | Stimulus | Expected result | Path exercised | Feature proved |
|---|---|---|---|---|
| UART-IO-001 | Send/receive characters through the platform's production serial/character frontdoor. | THR reaches external sink; external source reaches RBR without debug APIs. | production I/O adapter | Integration functionality |
| UART-TIME-001 | Queue several RX chars, read one just before timeout, then sample exact boundaries. | Timeout is restarted/cleared according to 16550 contract and remaining occupancy. | timeout event/RBR | Temporal correctness |
| UART-IRQ-001 | Assert all six real interrupt sources simultaneously, then clear one at a time. | IIR reports 7→3→6→2→1→0 priority exactly; IRQ remains until all clear. | `interrupt_id`, read side effects | Arbitration |
| UART-RX-001 | Fill RX FIFO to configured depth and inject one extra errored char. | Documented drop/overwrite policy, OE, ERRF, count, and original data are exact. | `deliver_rx_char` | RX boundary |
| UART-RX-002 | Place errors at several FIFO positions and sequence LSR/RBR reads. | PE/FE/BI front status, ERRF, error output, and IRQ transitions are exact. | LSR/error helpers | Error persistence |
| UART-TX-001 | Disable divisor, fill TX to capacity+1, then enable divisor/reset FIFO. | Exact capacity/drop order, history, THRE/TEMT, IRQ, and DMA transitions. | THR/drain/FCR | TX boundary |
| UART-FCR-001 | Cover all trigger selectors and FIFO enable/reset toggles at multiple CCI depths. | Trigger clamps correctly; FIFO/state resets are complete. | `update_rx_trigger`, `fcr_write` | FCR/ECR matrix |
| UART-MODEM-001 | Toggle all modem inputs and both loopback modes, including RI rising/falling. | Levels/deltas/output precedence match independent truth table. | modem methods/outputs | Modem behavior |
| UART-TLM-001 | Normal target protocol matrix for null/BE/streaming/alignment/range/command. | Exact errors and no side effects on rejected requests. | `b_transport` | TLM safety |
| UART-WRAP-001 | Equivalent protocol/reset/hole/first-last-entry matrix on wrapper. | Masks, reset, RAZ/WI, and byte counts are exact. | wrapper TLM/regmap | Wrapper CSR contract |
| UART-WRAP-002 | Drive actual log events and interrupt enable/test/status. | Write address/entries evolve and an observable IRQ follows enable/status. | missing log-engine datapath | Wrapper functionality |
| UART-DBG-001 | Debug read/write/ignore and malformed requests. | Side effects and byte counts match an explicit debug contract. | `transport_dbg` | Debug semantics |
| UART-CCI-001 | Boundary FIFO configs and exact mutable delay measurements. | Invalid bounds fail narrowly; transactions use updated delay. | constructor/CCI | Configuration |

## Verdict

**Strong register-level breadth, but incomplete end-to-end UART assurance.** The suite validates many 16550-visible states, yet ordinary character flow is entirely backdoor-based, timeout sequencing has a concrete gap, malformed TLM requests are unsafe, and `uart_wrap` lacks the log-engine behavior its CSRs imply.
