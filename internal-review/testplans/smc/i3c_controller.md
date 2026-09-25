# SMC `i3c_controller` test audit and remediation plan

## Scope and audit basis

Static audit only; no model or test code was changed.

Audited files:

- `smc/peripherals/i3c_controller/include/i3c_controller.h`
- `smc/peripherals/i3c_controller/src/i3c_controller.cpp`
- `smc/peripherals/i3c_controller/test/i3c_controller_tb.cpp`
- `smc/peripherals/i3c_controller/test/i3c_controller_cov_tb.cpp`
- `smc/peripherals/i3c_controller/test/i3c_controller_neg_tb.cpp`
- `smc/peripherals/i3c_controller/test/CMakeLists.txt`
- `smc/peripherals/i3c_controller/CMakeLists.txt`
- `smc/peripherals/i3c_controller/run_tests.sh`
- `smc/peripherals/i3c_controller/doc/test_plan.adoc`
- `smc/scripts/enforce_line_coverage.sh`

## Behavior inventory

- Multi-instance target aperture: `num_instances × 0x1000`; per-instance HCI/PIO CSRs plus DAT 0x300–0x3ff and DCT 0x400–0x4ff windows.
- Unimplemented in-aperture locations are RAZ/WI; out-of-aperture accesses fault.
- HCI command port assembles two 32-bit writes into one descriptor; command, response, TX, RX, and IBI queues are modeled.
- Descriptor execution resolves the DAT dynamic address and invokes a public bus-model callback after a transfer event.
- Private read/write and CCC read/write kinds, response TID/length/error encoding, FIFO underflow/overflow, command/response full handling, abort, and reset controls are implemented.
- PIO threshold status combines latched bits and computed levels. IRQ is the OR of enabled signal bits from PIO and HC status.
- Architectural active-low reset rebuilds per-instance state while preserving attached bus callbacks.
- `transport_dbg` supports side-effect-free DAT/DCT access only. Public `dbg_read`, `inject_ibi`, and `set_bus_model` are additional back doors.
- CCI parameters cover instance/FIFO sizing and mutable access/transfer delays.
- Bus output pins are held at abstract idle levels; recovery outputs are always false.

## Existing test classification matrix

| Area | Existing coverage | Classification |
|---|---|---|
| Core registers/tables | Reset values, representative RW/RO/W1C, DAT/DCT | Semantic but not exhaustive |
| HCI commands | Private read/write, CCC write/read, NACK/error, short read | Semantic through MMIO and callback |
| Queue errors | TX underflow/overflow, RX overflow, command/response full | Semantic |
| Interrupts | Response-ready, HC force, selected threshold levels | Semantic, some enable semantics not isolated |
| Reset/abort | Architectural reset, soft reset, queue resets, abort | Semantic; race coverage absent |
| Multi-instance | Basic isolation across three/two instances | Semantic |
| IBI | Direct `inject_ibi`, readout, queue-full/OOR | Direct back door; no external bus path |
| Debug | DAT/DCT debug access and broad `dbg_read` branch sampling | Mixed; substantial coverage-only content |
| TLM negative | Width, alignment, streaming mismatch, BE, aperture, command | Good baseline; pointer/DMI gaps |
| Timing/CCI | Handle discovery/mutation, waits longer than default transfer delay | No exact timing proof |
| Build/coverage | Three benches merged for coverage | Coverage-completion bench explicitly targets uncovered branches |

## Evidence findings

### Critical

1. **The canonical AXI extension is included but never read or tested.**  
   `i3c_controller.h:150` includes `smc_axi_extension.h`, but `b_transport` (`i3c_controller.cpp:700-753`) never calls `get_extension`. This violates the repository’s SMC sideband contract for inbound transactions. No test attaches the canonical extension. Add extension extraction and a test matrix for source ID, privilege, security, fetch, lock, AXI ID/user, plus absent-extension behavior.

### High

2. **Closed (2026-09-24) — ASan no longer ignores auxiliary-bench exits.**  
   Negative and coverage benches contribute to the phase exit. SMC orchestrator ASan PASS. UBSan remains disabled (finding 3).

3. **UBSan is not enabled.**  
   `CMakeLists.txt:41-45` enables only `-fsanitize=address`. The required undefined-behavior gate is absent.

4. **The “mutable” transfer delay is actually cached at construction.**  
   `xfer_delay_ns_p_` is documented mutable (`i3c_controller.h:126`, `i3c_controller.cpp:74-77`), but `xfer_delay_` is initialized once at `i3c_controller.cpp:129` and reused by `schedule_xfer`/chaining at `:187-189,373-375`. Tests never mutate it. Either make it immutable or re-read it at each scheduling decision and test exact timestamps.

5. **HC status-enable semantics are not implemented or independently tested.**  
   IRQ computation uses `intr_status & intr_signal_enable` only (`i3c_controller.cpp:294-297`), ignoring `intr_status_enable`. The coverage bench writes both status-enable and signal-enable together (`i3c_controller_cov_tb.cpp:463-478`), so it cannot detect this. The expected HCI relationship must be confirmed against the spec and tested with all four combinations.

6. **Command attribute and descriptor legality are effectively ignored.**  
   `cmd_attr(desc)` is explicitly discarded (`i3c_controller.cpp:392-394`); every descriptor is treated as a regular transfer. There are no malformed/reserved attribute, unsupported command, invalid CCC, or descriptor-field boundary tests. This can make invalid firmware commands look successful.

### Medium

7. **The coverage-completion bench contains explicit structural shortcuts.**  
   It is named and documented as targeting uncovered branches (`i3c_controller_cov_tb.cpp:4-38`, `test/CMakeLists.txt:34-40`). Sections 35–38 suppress warnings, call OOR/debug APIs, dump state, and sample switch branches (`i3c_controller_cov_tb.cpp:600-663`) without proving externally observable HCI behavior. These tests are useful for implementation coverage but should be labeled structural and excluded from claims of functional completeness.

8. **All warnings are globally suppressed in the coverage bench.**  
   `i3c_controller_cov_tb.cpp:708-713` changes every `SC_WARNING` to `SC_DO_NOTHING` to cover one OOR warning. This can hide unrelated warnings. Scope and restore the action for the expected report ID only.

9. **Null data pointers and DMI state are not validated.**  
   `b_transport` memcpy paths at `i3c_controller.cpp:727-737` assume a valid pointer. The negative bench covers many payload fields (`i3c_controller_neg_tb.cpp:160-183`) but not null pointers, zero BE length with non-null pointer, stale `dmi_allowed`, or `get_direct_mem_ptr`.

10. **Streaming-width policy is stricter than several sibling models but only one mismatch is tested.**  
    The target requires `streaming_width == length` (`i3c_controller.cpp:706-710`). Cover 0, less than, equal, and greater than length, and document whether `streaming_width > data_length` should be accepted under TLM2.

11. **Response length silently truncates to 12 bits.**  
    `make_response` masks `data_len` to 0xfff (`i3c_controller.cpp:50-56`), while command length is 16 bits. There are no 4095/4096/65535 tests or an explicit oversize error policy.

12. **Read-data contract permits short reads as success and truncates overlong callback data.**  
    `process_command` uses `min(read_data.size(), requested)` and returns success unless callback error says otherwise (`i3c_controller.cpp:446-467`). The suite proves a three-of-eight short read succeeds (`i3c_controller_cov_tb.cpp:685-696`) but does not reconcile that with `I3cShortReadErr`, or test overlong data. Add command flags/policy-specific short-read expectations.

13. **Zero-length and non-DWORD-length transfers need a complete semantic matrix.**  
    Existing tests use zero-length commands to fill response queues and a two-byte CCC transfer. Cover lengths 0, 1, 3, 4, 5, FIFO capacity in bytes, capacity+1, 4095, and 4096 for both directions and all four transfer kinds.

14. **Queue threshold and interrupt tests do not isolate enable/status behavior.**  
    Tests read level bits and one IRQ path, but do not cover each threshold at below/equal/above, threshold encodings 0 and max, W1C while level remains true, status-enable versus signal-enable, enable-after-pending, and instance isolation for IRQs.

15. **Pending transfer/reset/abort ordering is weakly covered.**  
    Shared `xfer_event_` and `xfer_pending_` drive all instances (`i3c_controller.cpp:362-377`). Missing: simultaneous commands on different instances, reset one tick before expiry, abort while an entry is already in `xfer_pending_`, bus disable before expiry, re-enable ordering, and reset/soft-reset with assembled low descriptor word.

16. **IBI validation is entirely through a direct method.**  
    `inject_ibi` (`i3c_controller.cpp:810-832`) is tested directly throughout both benches. Add an integrated target-to-controller binding or clearly state that this API is the modeled external I3C interface; test address masking, payload lengths 0/1/3/4/5/255/256, descriptor length truncation, ordering with responses, and IRQ threshold transitions.

17. **Register masks and reset/access contracts are sampled, not exhaustive.**  
    Important gaps include HC_CONTROL RESUME/HALT/HOT_JOIN/I2C bits, controller/stby address masks, DCT ENTDAA boundaries, all RO writes, PIO_CONTROL ABORT behavior, DAT/DCT first/last words, and every boundary around 0x4ff/0x500/0xffc/next instance.

18. **Outputs and mode pins are barely asserted.**  
    Bus/recovery outputs are bound but never checked after reset, traffic, abort, or mode-control writes. Since the abstraction promises constant idle values, assert that contract for every instance.

### Low

19. **Debug oracles duplicate implementation branches.**  
    `dbg_read` is sampled directly at `i3c_controller_cov_tb.cpp:640-663`, and several CSR values are compared between `b_transport` and the same backing state. Prefer architectural register/port observations for semantics and reserve debug tests for debug API conformance.

20. **Fatal/error report actions are globally changed in the negative bench.**  
    `i3c_controller_neg_tb.cpp:114-119` catches constructor reports but leaves broad action changes in place. Restore actions after constructor probes.

21. **Coverage is aggregate `src/`, not per touched file/header.**  
    `run_tests.sh:227-233` reports tests and headers as well, but the shared gate checks aggregate `.cpp` under `src/`. Inline header logic and a weakly covered touched file can escape the stated per-file criterion.

## Missing scenario inventory

### TLM2, aperture, sideband, debug/DMI

- Null pointer, BE pointer with length 0, stale response/DMI, every streaming-width relation, exact delay on success/error.
- `transport_dbg` bad command, null pointer, BE/streaming policy, DAT/DCT first/last entry, 0x500 boundary, last instance and first byte beyond aperture.
- Canonical AXI extension attached/absent and all fields; payload reuse/extension clone if used by platform bridges.
- Explicit DMI denial and unchanged register state on malformed traffic.

### HCI command/response

- Every command attribute and reserved encoding; invalid DAT index/address-valid state; unsupported CCC.
- All four transfer kinds at length boundaries and non-DWORD packing.
- TX underflow that leaves existing words intact versus consumed; RX overflow with prefilled queue; response overflow and recovery.
- Short-read allowed/disallowed policy, overlong callback, callback ACK+error combinations, no bus model.
- TID 0/15, length 0/0xfff/0x1000/0xffff, and response-field truncation/error behavior.
- Partial descriptor reset/abort, odd number of command-port writes, three or more consecutive writes, per-instance assembly isolation.

### Reset, events, threads, timing, interrupts

- Architectural reset while pending event/IRQ/IBI/partial descriptor exists.
- Every RESET_CONTROL bit alone and combined; verify CSR-preservation versus queue reset.
- ABORT/RESUME ordering before and after scheduled execution.
- Exact mutable access and transfer delay, simultaneous instance events, FIFO-order fairness.
- Every PIO and HC interrupt through status, status-enable, signal-enable, IRQ, W1C, and level reassertion.
- IBI/response/RX/TX/CMD thresholds at n-1/n/n+1 and max encodings.

## Proposed test matrix

| ID | Layer | Scenario | Oracle |
|---|---|---|---|
| I3C-TLM-01 | MMIO | Full malformed generic-payload matrix | Exact response, delay, DMI, no state mutation |
| I3C-TLM-02 | MMIO | Canonical AXI extension attached/absent | Field integrity and documented policy |
| I3C-TLM-03 | Debug | DAT/DCT boundaries and malformed debug traffic | Byte count and no FIFO/IRQ side effects |
| I3C-REG-01 | MMIO | Every CSR reset/mask/RO/W1C contract | Independent RDL-derived expectations |
| I3C-MAP-01 | MMIO | First/last word of each region/instance | No aliasing, correct RAZ/WI/fault policy |
| I3C-CMD-01 | HCI | Descriptor attribute/field validity matrix | Response error/TID/length |
| I3C-CMD-02 | HCI | Four transfer kinds × length packing boundaries | Callback request and FIFO bytes |
| I3C-CMD-03 | HCI | Short/long/error callback results | Spec-correct error and actual length |
| I3C-QUEUE-01 | HCI | Pre-filled TX/RX/CMD/RESP queues at capacity edges | Atomic consumption and status |
| I3C-IBI-01 | External adapter | Payload/address/queue/ordering matrix | IBI_PORT descriptors and IRQ |
| I3C-IRQ-01 | MMIO/signals | Every status/enable/signal combination | `irq_o` and status persistence |
| I3C-TIME-01 | Kernel | Mutable access/xfer delay and multi-instance ordering | Exact timestamps |
| I3C-RST-01 | Kernel | Reset/abort/disable around scheduled event | No stale response or orphan pending entry |
| I3C-OUT-01 | Signals | Idle bus/recovery abstraction | Outputs remain contract values |
| I3C-BUILD-01 | Runner | Auxiliary bench failure injection | **Closed 2026-09-24** — ASan phase fails on auxiliary nonzero |
| I3C-BUILD-02 | Sanitizer | Test-only UB canary | UBSan causes failure |

## Verdict

**Not protocol-complete despite broad line/branch exercise.** The suite has good descriptor-level functional breadth. Ignored auxiliary-bench exits in ASan mode are **closed (2026-09-24)**. Remaining strongest concerns: missing canonical AXI sideband handling, absent UBSan, a falsely mutable transfer-delay parameter, unisolated interrupt-enable semantics, and explicit coverage-completion tests that directly traverse debug/warning branches. Treat current coverage as implementation exercise, not HCI/TLM conformance.
