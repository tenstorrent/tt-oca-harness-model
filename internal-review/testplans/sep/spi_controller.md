# SEP `spi_controller` test audit

## Scope and audited files

- Model: `include/spi_controller{,_base,_interface,_register}.h`, `src/spi_controller{,_base}.cpp`
- Tests: all files under `test/inc` and `test/src`, including `test_func000.cpp`–`test_func010.cpp` and `test_coverage.cpp`
- Configuration/build: `config/*.json`, `config/accellera_config.ini`, `CMakeLists.txt`, `run_tests.sh`, `doc/test_plan.adoc`

## Behavior inventory

- Fourteen 32-bit CSRs: interrupts, control/status, per-CS configuration, command, FIFO ports, error/event controls.
- Command FIFO, TX/RX FIFOs, standard/dual/quad directions, CSAAT state, per-CS `CONFIGOPTS`, byte order, software/hardware reset.
- Transaction SC_THREAD stalls on TX data or RX space, invokes `spi_if`, advances a quantum keeper, and updates status/events.
- Error/status paths include invalid CSID/speed/direction, command-full, byte-enable access error, TX overflow/underflow, RX underflow, W1C error status, and test-forced interrupts.
- Combined and split interrupt outputs plus a level-sensitive DMA trigger.
- Timing calculation uses CLKDIV, CS lead/trail/idle, speed, segment length, and configurable core period.

## Existing test classification matrix

| Suite | Covered behavior | Path | Classification |
|---|---|---|---|
| FUNC-000 | HW/SW reset and defaults | CSR/pins | Mixed; one optional result |
| FUNC-001 | TX + dummy + RX fast-read sequence | CSR to `spi_if` stub | Positive |
| FUNC-002 | dual/quad/bidir, invalid speed/bidir, mode fields, length 1/255 | CSR/stub | Mixed; mode checks flawed |
| FUNC-003 | TX and RX stalls/resume, 300-byte RX | CSR/thread/stub | Positive/concurrency |
| FUNC-004 | event/error interrupts and masks | CSR/signals | Mixed |
| FUNC-005 | errors/recovery/byte enables/CSID | CSR/signals | Negative; one timing skip |
| FUNC-006 | per-CS configuration | CSR/stub | Positive/configuration |
| FUNC-007 | CSAAT/multi-segment, 1 KiB TX and 2 KiB RX split into chunks | CSR/stub | Positive; config skip |
| FUNC-008 | SPIEN/OUTPUT_EN gating | CSR/thread | Positive |
| FUNC-009 | command FIFO/full/CMDQD | CSR/thread | Boundary |
| FUNC-010 | DMA watermarks and reset | CSR/signals | Positive/boundary |
| Coverage extras | endian, forced IRQ, immediate events, reset, overflow/underflow | mixed | Coverage-targeted |

## Coverage-shortcut findings

1. **Critical — SPI mode tests use the wrong register bit positions and never verify the downstream configuration.** `CONFIGOPTS` defines CPOL/CPHA/FULLCYC at bits 31/30/29 (`spi_controller_register.h:272-280`), but FUNC-002 writes/reads bits 0/1 and shifts CLKDIV into bits 16+ (`test_func002.cpp:660-679,727-743,793-810`). Those checks merely read back CLKDIV/CSNIDLE bits. `spi_config_t` has no CPOL/CPHA/FULLCYC fields (`spi_controller_interface.h:41-47`), so these advertised modes cannot affect `spi_if`.
2. **Critical — full command length is unsafe and untested.** COMMAND exposes a 20-bit LEN (`spi_controller_register.h:339-347`), then `cmd_len+1` is assigned to 16-bit `spi_segment_t::len` (`spi_controller.cpp:1066-1073,1162-1169`) and used with fixed 512-byte stack buffers (`:494-498`). No validation rejects `>512`; tests top out at 300 bytes per command. This leaves truncation and stack-buffer overflow risks.
3. **High — register helpers pre-mark transactions OK and do not validate the target response.** `read_register_32`/`write_register_32` set `TLM_OK_RESPONSE` before transport and explicitly skip checks (`spi_controller_test.cpp:52-91`); malformed or rejected transactions can look successful.
4. **High — the assertion helper does not fail the test.** `assert_register_value` only logs on mismatch (`spi_controller_test.cpp:121-139`) and does not increment a failure counter, throw, report, or return failure.
5. **High — direct DUT parameter mutation bypasses configuration/frontdoor behavior.** Big-endian coverage calls `dut->ByteOrder.Set_param(...)` (`test_coverage.cpp:35-37,85-87`) and then fails to assert RX data (`:79-84`). This covers branches without proving byte order.
6. **High — coverage cases contain no-assert paths.** RXWM is marked “code path tested” without checking the interrupt (`test_coverage.cpp:276-293`); signal-update-during-reset prints PASS unconditionally after only checking CSR reset (`:445-483`).
7. **High — permissive outcomes hide missing behavior.** FUNC-000 counts either error/no-error as pass (`test_func000.cpp:170-174`); FUNC-005 skips CMDBUSY timing without failure (`test_func005.cpp:121-126`); FUNC-007 counts a NumCS-dependent skip as passed (`test_func007.cpp:537-541`).
8. **High — interrupt checks use logical OR between CSR and pin.** Examples accept either `INTR_STATE` or IRQ assertion (`test_func004.cpp:111-121,185-194,346-356`). A broken pin or broken CSR can pass as long as the other works; both and `irq_o` should be asserted independently.
9. **Medium — the clock is a constant boolean, not a clock.** `configure_clock` writes `clk_i=1` once while claiming 100 MHz (`testbench.cpp:216-222`). No edge-sensitive behavior or clock-stop/restart is tested.
10. **Medium — timing verification uses generous sleeps, not expected delay.** Tests poll/wait fixed microseconds; none compare `calculate_segment_delay`, quantum-keeper advance, speed ratio, lead/trail/idle, or CSAAT timing.
11. **Medium — no generic TLM protocol matrix.** There is no bad offset, unaligned access, zero/short/long length, streaming-width mismatch, null pointer, IGNORE command, `transport_dbg`, DMI, or payload extension test.
12. **Medium — coverage and sanitizer setup can overstate readiness.** Coverage targets aggregate all `src/*`/`include/*` (`CMakeLists.txt:183-188` and shared helper); standalone `run_tests.sh:29-71` neither enforces sanitizer logs/leaks nor rejects combined ASAN/coverage options. The top-level orchestrator is stronger, but standalone claims are broader than its checks.

No private-public macro, direct private callback invocation, SC report suppression, or compile-time test-only branch in model code was found.

## Missing scenarios

- Every legal command length boundary: 1, 2, 3, 4, 255, 256, 511, 512; rejection of 513 and all representable oversized LEN values.
- Exact TX/RX partial final words in both byte orders for lengths modulo 4 = 1, 2, 3.
- CPOL/CPHA/FULLCYC actually passed to the SPI target, or explicit documentation that this abstraction omits them.
- Timing math for all speeds, CLKDIV 0/max, CS timing 0/max, CSAAT idle omission, zero clock period, and quantum synchronization.
- `spi_if` failure return; current code still advances timing and may leave state/events inconsistent.
- Clock low at command arrival, low→high recovery, and real clock edges.
- All legal/illegal TXDATA strobe patterns, including null byte-enable conventions and partial-address writes.
- Separate assertion/deassertion checks for `INTR_STATE`, split IRQ pins, combined `irq_o`, EVENT_ENABLE, ERROR_ENABLE, INTR_ENABLE, and INTR_TEST.
- ALERT_TEST semantics, currently storage-only and untested.
- RX underflow W1C/recovery, ACCESSINVAL enable semantics, queue/error interactions, and all reset-during-stall races.
- CCI boundaries: NumCS 0/1/many, FIFO/CmdDepth 0/1/max, mutable timing/byte order, mismatched testbench presets.
- TLM debug/DMI/extensions/delay/response and malformed payloads.

## Proposed testcases

| ID | Stimulus | Expected result | Path | TLM feature |
|---|---|---|---|---|
| SPIH-001 | LEN encodings 0,1,2,3,254,255,510,511 | Exact byte count and final-word packing | CSR → `spi_if` | command boundary |
| SPIH-002 | LEN 512 and larger through maximum field | Explicit CMDINVAL/rejection; no truncation or buffer overwrite | CSR/thread | negative/safety |
| SPIH-003 | Capture all `spi_config_t` fields for four SPI modes | Correct CLKDIV/CS timings; CPOL/CPHA/FULLCYC behavior explicitly verified | CSR → stub | configuration |
| SPIH-004 | Lengths modulo 4 in little/big endian | Exact TX bytes and RX words | CSR/FIFO/stub | data packing |
| SPIH-005 | Stub returns false | Error/status/FSM/queue policy is deterministic | thread/stub | downstream failure |
| SPIH-006 | Compare elapsed/keeper time for speed/divider/timing extremes | Exact formula and CSAAT idle difference | CSR/thread | timing/delay |
| SPIH-007 | Real toggling `sc_clock`; stop/restart clock | Defined stall/error/recovery behavior | clock/thread | event/clock |
| SPIH-008 | Every valid and invalid byte-enable pattern | Valid data ordering; invalid sets ACCESSINVAL and does not enqueue | CSR TXDATA | byte enables |
| SPIH-009 | Assert each interrupt cause independently | CSR bit, split pin, and `irq_o` all agree; masks clear all three | CSR/signals | interrupt |
| SPIH-010 | Queue full at CmdDepth 1 and default; clear error | Exact READY/CMDQD/CMDBUSY and resumed command order | CSR/thread | queue boundary |
| SPIH-011 | SW/HW reset during TX wait, RX-full wait, and downstream call | No stale pop, lost command, stuck event, or stale output | pin/CSR/thread | reset/race |
| SPIH-012 | Bad offset/alignment/length/stream/null/IGNORE | Correct response status and no side effects | target socket | TLM protocol |
| SPIH-013 | `transport_dbg`, DMI request, arbitrary extension | Explicit support/refusal; no side effects unless documented | target socket | debug/DMI/extensions |
| SPIH-014 | NumCS 2 configuration | Independent CONFIGOPTS and real multi-device CSAAT behavior | CCI + CSR | configuration |
| SPIH-015 | ALERT_TEST read/write | WO mask and modeled/unmodeled alert policy | CSR | register behavior |

## Verdict

**Fail pending critical fixes/tests.** The suite is broad and mostly frontdoor-driven, but it misses an unsafe command-length range and its SPI mode tests verify the wrong bits while the abstraction drops CPOL/CPHA/FULLCYC entirely. Permissive checks, pre-OK TLM responses, direct parameter mutation, and unasserted coverage paths materially overstate functional coverage.
