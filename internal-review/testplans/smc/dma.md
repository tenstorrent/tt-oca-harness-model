# SMC DMA Test Audit and Plan

## Files audited

- Model: `smc/peripherals/dma/include/dma.h`, `src/dma.cpp`
- Tests: `test/dma_tb.cpp`, `test/dma_neg_tb.cpp`
- Build/coverage: `CMakeLists.txt`, `test/CMakeLists.txt`, `run_tests.sh`
- Shared coverage/ASan gates invoked by the runner.

## Behavior inventory

- CCI: immutable channel count, burst size, base address; mutable register and transfer delays.
- Sockets/process: 32-bit register target, 64-bit data initiator, asynchronous transfer thread/event.
- Registers: CONFIG; per-channel STATUS/NEXT_ID/DONE; shared 64-bit source, destination, length, strides, repetitions split into halves.
- Transfer: NEXT_ID read latches descriptors, marks channel busy, queues work; chunked read/write master transactions; 2D repetition/stride; done count and busy clear.
- TLM target: null-pointer/length/streaming/range/command/decode checks, debug read/write, delay, DMI denial, absolute/offset addressing.
- Master TLM: read then write each chunk; waits annotated delays; checks response; attaches canonical AXI extension with `SMC_ID`.

## Existing scenario matrix

- **Genuine** — Register defaults/readback, 64-bit half assembly, simple/chunked copy, timing observation, status/done, two queued channels, 2D strides, source error, decode holes/window, and debug read/write are exercised through the register socket (`dma_tb.cpp:231-575`).
- **Genuine** — Destination bytes are checked independently against initialized source patterns for 1D and 2D transfers (`dma_tb.cpp:304-333,445-482`).
- **Partial** — Two channels are queued but one transfer thread serializes them; no overlapping busy interval or scheduling-order assertion proves concurrency semantics (`dma_tb.cpp:402-441`).
- **Partial** — Source failure checks only that DONE increments; destination remains unchecked and no error status exists (`dma_tb.cpp:485-504`).
- **Partial** — Transfer timing uses broad waits around DONE/busy, not exact master delay accounting across multiple chunks/repetitions (`dma_tb.cpp:336-373`).
- **Coverage-only** — The negative bench claims source/destination `copy_chunk` error coverage but explicitly never calls `sc_start`; queued transfers therefore never execute and neither master error path is reached (`dma_neg_tb.cpp:4-16,250-280`).
- **Coverage-only** — Constructor fatal helper accepts any exception, not the expected report (`dma_neg_tb.cpp:63-72,134-150`).
- **Coverage-only** — Zero-length NEXT_ID is read twice with only response status checked; the returned data is not asserted in the first request and no state invariants are checked (`dma_neg_tb.cpp:239-247`).

No private-access macro or direct private method call was found. Tests do inspect the backing memory directly, which is appropriate as an independent transfer oracle.

## Findings

- **Critical — negative “copy failure” tests never run the transfer thread.** The bench performs all transactions during elaboration and returns without `sc_start` (`dma_neg_tb.cpp:12-16,123-287`). The source/destination failure requests only enqueue work; `transfer_thread`, `execute_transfer`, and `copy_chunk` are never entered by those cases.
- **High — failed transfers are counted as completed with no error indication.** `execute_transfer` returns early on failed chunks but returns no status; the thread always increments DONE and clears busy (`dma.cpp:349-365,396-413`). The primary test codifies this as expected behavior (`dma_tb.cpp:500-503`). Firmware cannot distinguish success from failure.
- **High — byte enables are ignored.** Target `b_transport` never checks byte-enable pointer/length (`dma.cpp:90-149`), and all tests provide null.
- **High — register alignment is not validated at the TLM boundary.** Misaligned addresses happen to miss decode and return ADDRESS_ERROR, not a defined BURST/alignment error (`dma.cpp:114-146`, `dma_neg_tb.cpp:213-216`).
- **High — `transport_dbg` accepts unsupported commands.** It returns 4 even when the command is neither read nor write because there is no final `else` rejection (`dma.cpp:151-171`). No test sends IGNORE to debug.
- **High — no reset port/process or runtime reset test exists.** Channel IDs, done counters, busy/pending descriptors, shared registers, and CONFIG have constructor initialization only (`dma.h:152-219`, `dma.cpp:24-85`). This is a major platform/reset integration gap.
- **High — outgoing AXI extension is unverified.** The model attaches only `source_id=SMC_ID` on read/write (`dma.cpp:437-443,466-472`); the memory target never inspects extension presence, type, source ID, default fields, or lifetime (`dma_tb.cpp:51-81`).
- **Medium — incoming target extension is fetched/discarded and untested** (`dma.cpp:124-128`).
- **Medium — read data is overwritten on decode miss.** `reg_read` initializes value to zero and the callback copies it before setting ADDRESS_ERROR (`dma.cpp:130-146,225-285`). Tests check only response status, not buffer preservation.
- **Medium — delay/error policy is under-specified.** Register delay is added even for in-window decode misses after processing, but not earlier validation failures (`dma.cpp:114-149`). Master delay is waited before response is checked (`dma.cpp:442-451,471-480`). No exact error timing test exists.
- **Medium — arithmetic limits are unguarded.** `repetitions+1`, `r*stride`, address addition, chunk length allocation, DONE/NEXT_ID increment, and data-length narrowing can wrap (`dma.cpp:361-362,382-400,404-425,431-433`).
- **Medium — busy-channel and descriptor-latching boundaries are missing.** No second NEXT_ID read occurs while the same channel is busy, and shared descriptors are not rewritten while a transfer is in flight to prove they were latched.
- **Medium — DMI and master protocol details are not tested.** Target DMI false is unchecked; downstream command, address, length, streaming width, byte enables, initial response, delay, and read-before-write ordering are not recorded/asserted.
- **Low — CONFIG is storage-only.** Tests validate readback but no transfer mode bit affects behavior, and there is no explicit unsupported-bit contract (`dma.cpp:243-246,300-303`).

## Missing scenarios

- Real source-read and destination-write errors after `sc_start`, including first/middle/final chunk and later repetition.
- Success/error completion semantics, error status/interrupt contract, partial destination contents, retry/abort policy.
- Runtime reset while idle, queued, reading, waiting delay, writing, and with multiple channels pending.
- Same-channel busy re-trigger, channel ID/done wrap, all channel boundaries, queue order, and fairness.
- Descriptor rewrite after launch and before execution; descriptor independence among queued channels.
- Exact chunk sequence for lengths 1, burst-1, burst, burst+1, multiple bursts; transfer delay per read/write.
- Zero length, maximum values, repetition overflow, address/stride overflow, overlapping source/destination.
- Target null/length/streaming/byte-enable/alignment/command/range/response/buffer/DMI matrix.
- Debug unknown command and malformed protocol attributes.
- Incoming and outgoing canonical AXI extension fields and lifecycle.
- Downstream target mutating delay, returning every response error, or returning incomplete.

## Proposed tests

1. **Instrumented master target:** log every master payload and extension. Expect exact read/write ordering, chunk addresses/lengths, streaming width, null byte enables, initial INCOMPLETE response, `SMC_ID`, unchanged remaining extension defaults, and consumed delay.
2. **Real error matrix:** run simulation and inject source or destination failures at chosen transaction indices. Expect a documented success/error result, exact DONE/busy behavior, and exact partial-write footprint. Do not treat unconditional DONE as success unless the RDL defines it that way.
3. **Busy/latching:** launch channel 0, immediately rewrite all descriptors and reread NEXT_ID for channel 0 and channel 1. Expect channel 0's second request rejected/zero, first transfer uses the original snapshot, and channel 1 uses the later snapshot.
4. **Chunk boundaries:** lengths `{1,B-1,B,B+1,2B+3}` and repetitions `{0,1,2}`. Expect exact transaction count, addresses, bytes, and total simulated latency.
5. **Reset suite:** add/use the platform reset contract and assert it at every transfer phase. Expect no stale master write/completion after reset and reset values for IDs/counters/registers.
6. **Target TLM matrix:** null pointer, widths, all alignments, streaming 0/mismatch, byte enables, valid/decode/OOB/overflow addresses, IGNORE, preseeded response/buffer/DMI. Check exact outcomes and no side effects.
7. **Debug matrix:** READ/WRITE/IGNORE with malformed geometry and pointer. Expect zero for unsupported commands and no transfer trigger unless explicitly documented.
8. **AXI extension:** attach populated extension on register requests and verify preservation; inspect every outgoing read/write for the canonical extension and correct source/security/protection policy.
9. **Overflow guards:** exercise near-maximum length/repetition/address/stride and ID/DONE wrap using bounded configurations or injected state hooks. Expect deterministic rejection/error rather than wrap or unbounded simulation.
10. **Concurrency/fairness:** queue several channels with delayed target responses. Assert execution order, busy overlap policy, no starvation, independent IDs/DONE, and descriptor integrity.

## Verdict

**Critical coverage-integrity and completion risk.** The main copy tests are useful, but the negative bench's advertised master-error coverage never executes. The model also reports failed copies as done, has no runtime reset, ignores byte enables, accepts invalid debug commands, and lacks any verification of its required outgoing canonical AXI extension.
