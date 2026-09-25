# SMC AVSBus Controller Test Audit and Plan

## Files audited

- Model: `smc/peripherals/avsbus_controller/include/avsbus_controller.h`, `src/avsbus_controller.cpp`
- Tests: `test/avsbus_controller_tb.cpp`, `test/avsbus_controller_neg_tb.cpp`
- Build/coverage: `CMakeLists.txt`, `test/CMakeLists.txt`, `run_tests.sh`
- Shared gates used by the runner: `smc/scripts/enforce_line_coverage.sh`, `smc/scripts/enforce_asan_clean.sh`

## Behavior inventory

- CCI: immutable command/readback FIFO depths; mutable register, transfer, and resync delays.
- Ports/processes: target socket, active-low reset, IRQ/GPIO outputs; reset, output recompute, resync event, and transfer thread.
- Registers: command/readback FIFOs; latest frame; normal/slave/FIFO status; sticky interrupt status, active-high disable mask, W1C clear; masked CFG0/CFG1/CONFIG.
- Protocol: command launch, default/callback response, BUSY/BAD_CRC retry, retry exhaustion, readback backpressure, forced resync, FIFO overflow/underflow.
- TLM: 32-bit target, `b_transport`, `transport_dbg`, decode errors, annotated delay, DMI denial, optional canonical AXI extension extraction.
- Public backdoors/helpers: slave callback, interrupt/readback injection, FIFO counts, `dbg_reg`, dump, field pack/unpack, CRC-3.

## Existing scenario matrix

- **Genuine** — Reset values, masked RW/RO/WO/W1C behavior, GPIO output, command-to-default-response flow, IRQ mask/clear, FIFO full/overflow/underflow, retry exhaustion, forced resync, canned read response, callback BUSY/BAD_DATA paths, and resync/transfer interaction are driven through `b_transport` and checked at registers/ports (`avsbus_controller_tb.cpp:166-348`, `avsbus_controller_neg_tb.cpp:237-393`).
- **Genuine** — Command/readback FIFO side effects are checked through bus reads/writes, including debug peek versus destructive readback pop (`avsbus_controller_tb.cpp:210-232`).
- **Partial** — Timing is exercised only by waiting comfortably past nominal deadlines; no transaction checks the exact `b_transport` delay or the before/at/after transfer and resync boundaries (`avsbus_controller_tb.cpp:218,320`).
- **Partial** — Retry exhaustion accepts `calls >= 3` although max-retries=2 has an exact observable attempt count of three (`avsbus_controller_tb.cpp:294-309`).
- **Partial** — Canonical AXI extension is extracted and discarded; no test attaches one or proves payload/extension preservation (`avsbus_controller.cpp:540-542`).
- **Coverage-only** — `dbg_reg` is called once for every switch arm, with several results discarded, solely to execute lines (`avsbus_controller_neg_tb.cpp:188-231`).
- **Coverage-only** — `inject_readback` manufactures a full readback FIFO overflow that normal command processing prevents via backpressure; useful as a hardware-event unit test, but not evidence for the bus-visible producer path (`avsbus_controller_neg_tb.cpp:275-288`).
- **Coverage-only** — Canned response expectations duplicate the DUT's command-code switch table, so the test and implementation can drift together (`avsbus_controller.cpp:292-302`, `avsbus_controller_neg_tb.cpp:327-348`).
- **Coverage-only** — CRC coverage is only `crc3(0)==0`; it does not validate the polynomial against independent vectors (`avsbus_controller_neg_tb.cpp:415-419`).

No `private`/`public` preprocessor override or direct private `reg_read`/`reg_write` call was found. Public backdoors are used extensively.

## Findings

- **High — malformed payload can dereference null.** `b_transport` and `transport_dbg` unconditionally `memcpy` the data pointer after width/address validation (`avsbus_controller.cpp:524-558,570-586`). Neither bench submits a null pointer.
- **High — byte enables and streaming width are silently ignored.** The production callback validates command, length, and address only (`avsbus_controller.cpp:526-538`); tests always use streaming width 4 and null byte enables (`avsbus_controller_tb.cpp:88-96,108-116`).
- **Closed (2026-09-24) — ASan no longer passes when the negative bench fails.** The runner records `NEG_EXIT` and fails if either binary is nonzero. SMC orchestrator ASan PASS.
- **Medium — no exact delay contract.** The model adds mutable `access_delay_ns` only on successfully decoded transactions (`avsbus_controller.cpp:559-562`), but tests neither seed/check delay nor define whether rejected transactions consume latency.
- **Medium — DMI behavior is only indirectly set.** Successful `b_transport` denies DMI (`avsbus_controller.cpp:563`), but no test starts with `dmi_allowed=true`, calls `get_direct_mem_ptr`, or checks rejected requests.
- **Medium — reset races are incompletely covered.** Reset cancels transfer and resync events and clears pending/retry state (`avsbus_controller.cpp:140-164`), but no command is reset while queued, transferring, retrying, readback-full, or resynchronizing.
- **Medium — response integrity is not tested.** The controller acts only on ACK and never validates response CRC; tests do not state whether bad response CRC must be accepted, retried, or flagged. CRC generation itself lacks independent vectors.
- **Medium — interrupt coverage is synthetic for several sources.** `SLAVE_ISSUED` is tested only with `inject_interrupt`, while `SLAVE_UNRESPONSIVE` is not tested and has no natural model path (`avsbus_controller.h:245-257`, `avsbus_controller_tb.cpp:243-252`).
- **Low — permissive constructor-fatal matcher.** `expect_fatal` accepts any C++ exception instead of checking `sc_report` severity/message (`avsbus_controller_neg_tb.cpp:57-65,451-464`).
- **Low — broad assertions hide exact behavior.** Retry count and retry counter use `>=` rather than exact values (`avsbus_controller_tb.cpp:307`, `avsbus_controller_neg_tb.cpp:287,319`).

## Missing scenarios

- Null data pointer; byte-enable pointer/length combinations; streaming width 0, less than, and greater than length.
- Exact initial response status transition, returned response status for all malformed requests, read-buffer preservation on errors, and DMI flag clearing.
- `get_direct_mem_ptr` denial and debug command/width/address/data-pointer matrix.
- Attached canonical `smc_axi_extension` with non-default source/protection/user/security fields; verify it is not replaced, corrupted, or retained incorrectly.
- Exact access delay and mutable CCI delay change; error-path delay policy.
- Reset during queued command, transfer delay, BUSY retry, resync, readback-full backpressure, and asserted IRQ.
- Retry boundaries for max-retries 0/1/255, BUSY versus BAD_CRC, ACK_BAD_DATA, exact total retry wrap, and callback invocation order.
- FIFO depths 1 and non-default values; simultaneous pop/unblock and command launch; repeated clear while level/sticky causes remain.
- Every status bit transition, all interrupt sources, exact IRQ masking polarity, partial W1C, reserved-bit masks, CFG1 pulse self-clear.
- Independent CRC vectors and response CRC policy.

## Proposed tests

1. **TLM payload matrix:** issue each malformed request through the socket. Expect COMMAND, BURST, ADDRESS, BYTE_ENABLE, or GENERIC error as specified; expect no register/FIFO mutation, no output transition, unchanged read buffer, and explicit delay/DMI policy.
2. **AXI extension preservation:** attach a canonical extension populated with source ID, AXI ID, `prot`, lock/fetch/security/user, and `axi_user`; perform valid read/write and rejected access. Expect normal register behavior and byte-for-byte unchanged extension fields.
3. **Exact timing:** seed delay to 7 ns and set access delay to 3 ns. Expect 10 ns after a valid request. Check transfer and resync at `T-ε`, `T`, and `T+ε`; mutate CCI and repeat.
4. **Reset race suite:** reset in each pending state. Expect reset register values, empty FIFOs, zero interrupt/retry state, idle bus, deasserted IRQ, enabled GPIO, and no stale completion after the old deadline.
5. **Retry truth table:** independently generate OK, BUSY, BAD_CRC, and BAD_DATA sequences. Check exact callback count, `MASTER_IS_RETRYING`, total retries, command occupancy, final frame, MAX_RETRIES IRQ, and next-command progress.
6. **FIFO depth/concurrency:** instantiate depth 1 and 2 configurations. Fill command/readback FIFOs, pop at the unblock boundary, and queue multiple channels of work. Check ordering, no duplication/drop beyond documented overflow, and sticky IRQ clear behavior.
7. **Independent protocol oracle:** use published AVSBus CRC vectors or a separately derived bit-serial reference, not the DUT equations. Validate command fields, canned response data/status, and CRC for every command-code class.
8. **Debug/DMI:** verify debug reads do not pop, debug writes have only documented effects, malformed debug calls return zero, and DMI is denied.

## Verdict

**High risk.** Functional happy paths are broad, but the suite leaves unsafe payload handling and major TLM protocol features untested. Coverage is inflated by switch-arm peeks, synthetic injection, duplicated canned-response logic, and permissive assertions. The ASan negative-bench exit hole is **closed (2026-09-24)**.
