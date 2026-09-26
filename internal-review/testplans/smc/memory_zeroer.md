# SMC `memory_zeroer` test audit and remediation plan

## Scope and audit basis

Static audit only; no source or test changes were made.

Audited files:

- `smc/peripherals/memory_zeroer/include/memory_zeroer.h`
- `smc/peripherals/memory_zeroer/include/smc_tlm_extensions.h`
- `smc/peripherals/memory_zeroer/src/memory_zeroer.cpp`
- `smc/peripherals/memory_zeroer/test/memory_zeroer_tb.cpp`
- `smc/peripherals/memory_zeroer/test/CMakeLists.txt`
- `smc/peripherals/memory_zeroer/CMakeLists.txt`
- `smc/peripherals/memory_zeroer/run_tests.sh`
- `smc/peripherals/memory_zeroer/doc/test_plan.adoc`
- `smc/scripts/enforce_line_coverage.sh`

## Behavior inventory

- 64-bit MMIO registers: destination, size, and control/status in a 0x18-byte aperture.
- Full-width, naturally aligned 8-byte accesses only; explicit byte enables rejected.
- `CTRL_STATUS.int_en` is software RW; busy/status is model-driven and software RO.
- Any control write with non-zero SIZE synchronously runs a zero-fill loop from inside the register `b_transport`.
- Outbound DMA writes are split by immutable CCI `chunk_size`; final chunk may be short.
- DMA failure stops the job, clears busy, and does not assert completion IRQ.
- Completion IRQ asserts only on success with `int_en=1`; clearing enable drops IRQ.
- Active-low reset clears registers and IRQ.
- Access delay is mutable via CCI. DMA target delay is accumulated in a local variable.
- The canonical AXI extension is reachable through a peripheral-local forwarding header, but the model does not use it.

## Existing test classification matrix

| Area | Existing coverage | Classification |
|---|---|---|
| Reset/registers | Reset, DEST/SIZE RW, control mask/status RO | Semantic, socket-driven |
| Zero operation | 16, 256, and 500-byte successful jobs | Semantic through outbound DMA socket |
| Chunking | Preset chunk=64 and 500-byte range checked | Semantic, but no per-transaction metadata assertions |
| Error | Out-of-range memory target causes no IRQ | Semantic, permissive about exact failure position/state |
| IRQ | Enabled/disabled completion and enable-clear | Semantic |
| TLM negative | Null, zero length, width, alignment, aperture, BE, streaming, command | Good baseline |
| CCI | Preset/mutation/immutability | Parameter value only; access delay not asserted |
| Debug | Dump path; inline debug helpers mostly indirect | Limited |
| Build/coverage | One bench; source gate | No independent negative bench |

## Evidence findings

### Critical

1. **Outbound DMA transactions do not carry the canonical SMC AXI extension.**  
   The model includes `smc_tlm_extensions.h` (`memory_zeroer.h:40`), but `perform_write_zeros` creates payloads without attaching `smc::smc_axi_extension` (`memory_zeroer.cpp:157-168`). The repository contract requires sideband metadata on every outgoing SMC transaction. No test inspects DMA sideband. Define the correct source ID/security/privilege/fetch/lock/user values and assert them for every chunk.

### High

2. **The nested blocking DMA operation makes busy unobservable and loses downstream timing.**  
   A control-register write invokes `trigger_job` synchronously from the register callback (`memory_zeroer.cpp:48-62`); busy is set, all DMA calls complete, and busy is cleared before the initiating MMIO `b_transport` returns (`:115-139`). Firmware cannot observe busy=1, reset cannot interrupt a job, and no event/thread timing exists. The DMA target’s annotated delay is accumulated only in a local variable (`:150,168`) and is neither waited nor returned to the MMIO initiator. Tests only inspect post-completion state. Decide whether the model is intentionally atomic; if busy is architectural, move work to a process/event and test it over simulated time.

3. **ASan mode does not include UBSan.**  
   `CMakeLists.txt:26-30` compiles with `-fsanitize=address` only. This fails the repository’s address+undefined sanitizer requirement.

4. **Address arithmetic can wrap.**  
   MMIO validation uses `adr + len > WINDOW_SIZE` (`memory_zeroer.cpp:212`), and DMA addresses use `addr + offset` (`:160,173`) without overflow guards. A destination near `UINT64_MAX` can wrap into low memory. Tests use only small addresses and one ordinary OOB address.

5. **There is no reset/job concurrency coverage because the implementation cannot model it.**  
   Reset is an SC_METHOD (`memory_zeroer.cpp:85-98`), but a job runs as an atomic call chain without `wait`. A reset asserted while software starts a large operation cannot be interleaved. This leaves reset cancellation, busy/IRQ race, and partial-write policy unspecified.

### Medium

6. **DMA payload conformance is not checked.**  
   The memory target validates only pointer/length/address and copies bytes (`memory_zeroer_tb.cpp:94-112`). It does not assert write-only command, exact chunk addresses/lengths, streaming width, null BE, response initialization, sideband, monotonic/non-overlapping spans, or accumulated delay input.

7. **Boundary and partial-failure behavior is incomplete.**  
   Tests cover 500 bytes with 64-byte chunks but not size 1, chunk-1/chunk/chunk+1, exact destination end, first-chunk failure, middle-chunk failure, last-chunk failure, or zero bytes before/after the requested span for every case. The current OOB test (`memory_zeroer_tb.cpp:413-421`) fails on the first request and only checks IRQ/control.

8. **Failure status is not software-visible.**  
   `perform_write_zeros` returns false and suppresses IRQ (`memory_zeroer.cpp:168-177,130-139`), but there is no error CSR or retained failure indication. Tests accept that. Verify this against the authoritative model/spec; otherwise firmware cannot distinguish failed completion from a job that never ran.

9. **A control write retriggers whenever SIZE remains non-zero.**  
   The callback calls `trigger_job` for every control write (`memory_zeroer.cpp:48-62,115-119`), including writes intended only to clear/set `int_en`. The suite demonstrates this incidentally at `memory_zeroer_tb.cpp:319-327` but does not test repeated enable changes, repeated same control value, or writes while conceptually busy. Confirm trigger semantics from RDL/reference.

10. **Annotated MMIO delay is not asserted.**  
    Driver helpers discard delay (`memory_zeroer_tb.cpp:124-151`), and CCI tests only inspect handle JSON (`:425-441`). Add exact delay assertions before and after mutation and clarify whether job/DMA delay contributes.

11. **Incoming sideband is ignored and untested.**  
    MMIO `b_transport` (`memory_zeroer.cpp:194-263`) never extracts the canonical extension. If upstream filtering owns authorization, tests should still attach and preserve the extension and verify absent-extension policy at platform level.

12. **No `transport_dbg` or explicit DMI contract exists.**  
    The model registers only `b_transport` (`memory_zeroer.cpp:67`). Add tests showing debug transfer behavior and DMI denial/default behavior are intentional; ensure successful and error responses clear stale `dmi_allowed` if required.

13. **Broad SystemC error demotion can hide unrelated failures.**  
    `memory_zeroer_tb.cpp:459-466` globally demotes `SC_ERROR` to display so an immutable CCI write can be attempted. Scope that expected report or test immutability without changing all SC_ERROR handling.

14. **The CTest failure regex is permissive.**  
    `test/CMakeLists.txt:7-11` explicitly says intentional CCI errors must not fail and only matches `^FAIL `. Non-prefixed fatal diagnostics can rely solely on process exit. Remove broad error demotion and use a precise expected-report mechanism.

### Low

15. **Debug getters expose backing state but do not prove architectural visibility.**  
    Inline helpers at `memory_zeroer.h:112-120` can become tempting coverage oracles. Prefer MMIO and IRQ/DMA observations; use debug only to test debug API.

16. **Coverage reporting includes tests but the gate is aggregate source-only.**  
    `run_tests.sh:251-254` reports model and test, while the shared gate checks aggregate `.cpp` under `src/`. Header inline behavior is not gated.

17. **The local `smc_tlm_extensions.h` is unnecessary indirection.**  
    It correctly forwards to the canonical header (`smc_tlm_extensions.h:13-18`) and does not duplicate the type, so this is not an ODR bug. Direct inclusion would make conformance clearer.

## Missing scenario inventory

### MMIO TLM2

- Non-zero incoming delay and exact additive result; post-CCI mutation result.
- Streaming widths 0, 1, 7, 8, and >8; BE pointer with BE length 0 and full mask; stale response/DMI.
- `UINT64_MAX`/wraparound addresses, final valid register, interior holes 0x04/0x0c/0x14, final aperture byte.
- Canonical AXI extension attached and absent; `transport_dbg`/DMI behavior.

### DMA command/response, address span, pointer/length

- Capture every outbound transaction and assert command=WRITE, pointer non-null, all bytes zero, `len` and `streaming_width` equal expected chunk, no BE, initial response incomplete.
- Sideband on every chunk, including final short chunk.
- Sizes 0, 1, C-1, C, C+1, 2C-1, 2C, and maximum practical test size.
- Destinations 0, unaligned, memory-end-size, memory-end-size+1, `UINT64_MAX-k`, and wrap attempt.
- Failure injection on first/middle/final chunk; verify number and spans of writes, busy/IRQ/error policy.
- Downstream delay per chunk and total job timing.

### Reset, event, thread, interrupt

- Observe busy assertion before first DMA and across multiple chunks.
- Reset before start, while busy, at final response, and after IRQ assertion.
- Concurrent/repeated CTRL writes while busy; SIZE/DEST writes while busy; retrigger policy.
- IRQ enable changed before completion, during completion delta, after completion, and while failure occurs.
- Reset output initialization at time zero and repeated reset assertions.

## Proposed test matrix

| ID | Layer | Scenario | Oracle |
|---|---|---|---|
| MZ-TLM-01 | MMIO | Complete malformed payload matrix | Exact response, no mutation, delay policy |
| MZ-TLM-02 | MMIO | Register holes and 64-bit wrap boundaries | Address error, no callback |
| MZ-TLM-03 | MMIO | Incoming AXI extension and DMI/debug policy | Preserved fields, explicit denial |
| MZ-REG-01 | MMIO | Reset/RW/RO/retrigger semantics | MMIO readback and DMA count |
| MZ-DMA-01 | Initiator monitor | Sizes around chunk boundaries | Exact command/address/length/data/BE/SW |
| MZ-DMA-02 | Initiator monitor | Canonical AXI sideband per chunk | Correct source/security/privilege fields |
| MZ-DMA-03 | Target fault injector | First/middle/final chunk error | Stop point, status, IRQ, partial span |
| MZ-TIME-01 | Kernel | Downstream annotated delays | Exact visible completion timestamp |
| MZ-BUSY-01 | Kernel | Busy lifecycle and concurrent MMIO | Busy observable; defined write policy |
| MZ-RST-01 | Kernel | Reset at each job phase | Cancellation/partial-write policy |
| MZ-IRQ-01 | Signals | Enable transitions around success/failure | Exact IRQ assertion/deassertion |
| MZ-CFG-01 | CCI | Chunk range and mutable access delay | Fatal boundaries and live behavior |
| MZ-BUILD-01 | Sanitizer | Test-only UB canary | UBSan fails the run |

## Verdict

**Functionally useful but not an adequate DMA/TLM protocol test suite.** Basic zeroing, chunking, MMIO errors, and completion IRQ behavior are covered. The critical gap is that outbound DMA payloads omit the canonical AXI extension. The synchronous nested implementation also makes busy and DMA timing unobservable, and arithmetic wrap, partial failures, reset concurrency, exact payload metadata, DMI/debug, and UBSan remain uncovered.
