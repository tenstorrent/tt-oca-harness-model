# mailbox Test Audit and Plan

## Audited files

- Model: `include/mailbox.h`, `include/mailbox_base.h`, `include/mailbox_register.h`, `include/mailbox_unit.h`, `src/mailbox.cpp`, `src/mailbox_base.cpp`.
- Tests: `test/inc/mailbox_basetest.h`, `test/inc/mailbox_test.h`, `test/inc/testbench.h`, `test/src/mailbox_basetest.cpp`, `mailbox_test.cpp`, `testbench.cpp`, and `test_mailbox_func001.cpp` through `test_mailbox_func007.cpp`.
- Build/coverage: `CMakeLists.txt`, `run_tests.sh`.

## Behavior inventory

- Two 64-bit CSR ports each contain WRITE_DATA, READ_DATA, STATUS, ERROR_FLAGS, WIRQT, RIRQT, IRQS, IRQEN, IRQP, and CTRL.
- Port 0 outbound FIFO is port 1 inbound and vice versa. Each deque has depth 8, FIFO ordering, full/empty state, and independent directions.
- `bus_access()` enforces decode/access policy: unmapped offsets, RO writes, CTRL reads, FIFO underflow, and overflow return an error; WRITE_DATA reads return `0xFEEDC0DE/OK`; empty READ_DATA returns `0xFEEDDEAD/error`.
- Narrow read response data is shifted by address lane and copied up to transaction length.
- WRITE_DATA/READ_DATA callbacks enqueue/dequeue and set sticky threshold/error status.
- STATUS is live: inbound empty, outbound full, write level `> WIRQT`, read level `> RIRQT`.
- ERROR_FLAGS is clear-on-read and accumulates read/write errors independently from sticky EIRQ.
- Thresholds saturate at 7 and are re-evaluated immediately.
- IRQS is W1C; IRQEN dynamically masks; IRQP is `IRQS & IRQEN`; an event-driven single writer drives both active-high interrupt outputs.
- CTRL write/read FIFO flushes are strobes and self-clear.
- Active-low reset clears both register sets, both FIFOs, shadows, access errors, and interrupts.
- `mailbox_unit_t<N>` routes one aperture to `2*N` port blocks, restores the original address, errors outside the aperture, and exports per-channel interrupts.

## Existing tests matrix

### Genuine

- Cross-port FIFO direction, FIFO ordering, full/empty boundaries, overflow/underflow sentinels and responses, error accumulation, clear-on-read, W1C, threshold strictness/saturation, dynamic IRQ masking, both interrupt outputs, reset during populated/error state, and both flush directions are substantially exercised through sockets.
- Explicit signal-level checks use delta-cycle waits for the interrupt driver.
- `mailbox_read/write()` exposes response status, allowing key negative paths to verify errors.
- Release/ASan/Coverage all execute the same test binary; coverage extraction includes model source and headers.

### Partial

- Basic RO protection compares before/after values but does not require an error response (`test/src/testbench.cpp:409-442`); READ_DATA reads can have side effects, so this is also a fragile access-policy test.
- Several tests use `register_read/write_*()` helpers that erase failed read data or ignore write response (`test/src/mailbox_test.cpp:40-45,75-80`). Those tests cannot distinguish a correct value from a failed transaction returning zero.
- “Simultaneous” bidirectional transfer is sequential software traffic, not concurrent threads (`test/src/test_mailbox_func003.cpp:346-454`).
- Reset interrupt-output check primarily proves IRQP=0; the captured `irq0_state/irq1_state` values are logged but not directly compared (`test/src/test_mailbox_func001.cpp:210-241`).
- The test headers/documentation claim variable FIFO depth and active-low/edge configurations, but the production constructor hardcodes depth 8 and active-high.

### Coverage-only

- No private/public macro or direct private callback call was found.
- Some checks are effectively coverage-only because failures are logged as warnings but do not fail:
  - ERROR_FLAGS not clearing in EIRQ test (`test/src/test_mailbox_func005.cpp:600-608`).
  - Multiple ERROR_FLAGS not both set (`test/src/test_mailbox_func005.cpp:960-967`).
  - IRQS unexpectedly clearing with ERROR_FLAGS (`test/src/test_mailbox_func005.cpp:993-998`).
- Flush tests deliberately discard the READ_DATA response after asserting emptiness (`test/src/test_mailbox_func007.cpp:177-183,350-356`), so the required sentinel/error semantics after flush are not proven.
- `mailbox_unit_t` has no executable tests despite being a public model surface.

## Shortcut findings

- **High — permissive warnings hide semantic failures:** the three FUNC-005 checks above do not change `test_passed`, allowing broken clear-on-read/accumulation/independence behavior to pass.
- **High — mailbox unit untested:** decode, channel/port routing, address restoration, aperture errors, reset fan-out, and interrupt-vector wiring in `include/mailbox_unit.h:76-142` have no test target.
- **Medium — helper masks bus failures:** `mailbox_test` converts failed reads to zero and ignores failed writes, yet many functional cases use it.
- **Medium — incomplete reset assertion:** interrupt signal values are read but not asserted in FUNC-001.
- **Medium — false configuration claims:** “minimum depth=2” and “maximum depth” tests operate the same hardcoded depth-8 DUT (`test/src/test_mailbox_func003.cpp:1050-1204`); they do not test configurable depth.
- **Medium — command classification hole:** `bus_access()` treats every command except WRITE as a read (`src/mailbox.cpp:285-289`), but no `TLM_IGNORE_COMMAND` test exposes this.
- **Low — report suppression:** only the IEEE deprecated warning is disabled (`test/src/testbench.cpp:718`); this is not hiding DUT failures, but any broader report-policy change should be forbidden.
- No expected-value clone of FIFO/IRQ algorithms, direct state poke, or offset-only coverage loop was found.

## Missing scenarios

- `mailbox_unit_t`: every channel and both blocks, block boundaries, last legal byte, holes between 0x50 and 0x7ff, aperture end, address restoration on success/error, independent channel state, interrupt vectors.
- TLM2: ignore/bad command, null pointer, zero and oversized length, unaligned offset/lane reads, byte enables, streaming width, DMI, `transport_dbg`, and annotated delay.
- Narrow access: upper/lower 8/16/32-bit reads and writes for all legal registers; sentinel lane extraction at nonzero byte offsets; partial W1C/CTRL writes.
- Reset: direct interrupt signal deassertion, reset coincident with pending event, repeated reset, startup held low.
- FIFO temporal/concurrency: two initiators in separate threads, alternating directions, clear concurrent with access, interrupt event coalescing in one delta.
- IRQ: all combinations of multiple pending bits, selective W1C while another remains, clear threshold status then retrigger without reset, IRQEN reserved masking.
- Access control: exact response for every RO/WO register on both ports; out-of-range and unaligned decode.
- No DMA functionality belongs to this IP.

## Proposed cases

1. **Access-policy matrix:** raw payload helper returns status and data without rewriting either. For every register/port, test legal and illegal read/write and assert exact response, data, and no unintended side effect.
2. **Malformed TLM matrix:** issue IGNORE, null pointer, lengths 0/1/2/4/8/9, unaligned lanes, byte-enable patterns, and streaming widths. Assert exact policy and unchanged FIFO/register state.
3. **Narrow lane semantics:** read WRITE_DATA and empty READ_DATA sentinels at offsets `+0..+7` with lengths 1/2/4; compare literal lane bytes. Verify partial threshold/IRQEN/W1C/CTRL writes.
4. **FIFO boundary oracle:** fill 0→7→8, attempt ninth write, drain 8→0, attempt ninth read; assert each response, each STATUS transition, FIFO data ordering, both error bits, and EIRQ.
5. **Interrupt composition:** set WTIRQ, RTIRQ, and EIRQ together, enable selected subsets, selectively W1C bits, and assert IRQS, IRQP, and `irq_o` after each delta.
6. **Reset event race:** create pending interrupt updates and populated FIFOs, assert reset before the driver delta, then assert both physical outputs low, all status/shadow state clear, and no stale event reassertion.
7. **Flush semantics:** after flush, perform READ_DATA and assert error plus `0xFEEDDEAD`; verify sticky threshold IRQS behavior is as specified rather than assuming a level drop clears it.
8. **Concurrent ports:** independent SC_THREAD producers/consumers exercise same-delta opposite-direction operations; compare against a small external queue oracle and prove no cross-direction corruption.
9. **Unit decode sweep:** instantiate `mailbox_unit_t<2>`, program unique data per channel/port, access first/last offsets and holes, and assert routing, response, original-address restoration, reset fan-out, and four interrupt outputs.
10. **Timing/DMI/debug:** target annotations must accumulate rather than be overwritten; assert declared DMI behavior and add debug transport tests or explicitly document that debug/DMI are unsupported.

## Verdict

The channel model has broad genuine semantic coverage, especially FIFO, threshold, error, and interrupt behavior. Confidence is **medium-high for ordinary 64-bit channel traffic**, but **low for the unit wrapper and TLM protocol robustness**. Permissive WARN-only checks are real false-pass risks and must be converted to failures before coverage numbers are trusted.
