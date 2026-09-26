# SMC `i2c_controller` test audit and remediation plan

## Scope and audit basis

Static audit only; the model and tests were not edited. The review compares the hand-written implementation with every standalone test and its build/coverage plumbing.

Audited files:

- `smc/peripherals/i2c_controller/include/i2c_controller.h`
- `smc/peripherals/i2c_controller/src/i2c_controller.cpp`
- `smc/peripherals/i2c_controller/test/i2c_controller_tb.cpp`
- `smc/peripherals/i2c_controller/test/i2c_controller_neg_tb.cpp`
- `smc/peripherals/i2c_controller/test/CMakeLists.txt`
- `smc/peripherals/i2c_controller/CMakeLists.txt`
- `smc/peripherals/i2c_controller/run_tests.sh`
- `smc/peripherals/i2c_controller/doc/test_plan.adoc`
- Shared gate used by the runner: `smc/scripts/enforce_line_coverage.sh`

## Behavior inventory

- Two target models are in scope: the per-core I2C controller and `i2c_wrap_ctrl`.
- Main target aperture: 0x200 bytes, 32-bit naturally aligned accesses; decoded registers through 0x80.
- Wrapper aperture: 0x10 bytes, three 32-bit `I2C_CTRL` registers.
- Register semantics include RW masks, RO/WI, WO/RAZ, W1C, read-clear, self-clearing FIFO reset strobes, threshold-derived level interrupts, and forced interrupts.
- Controller path: FMT descriptor queue, delayed drain event, segment assembly, repeated START, read/write callback, NACK halt/resume, RX overflow, completion interrupt.
- Target path: public `target_write`/`target_read` back doors, target-address matching, ACQ/TX queues, NACK count, START/STOP events.
- Reset clears registers, FIFOs, events, pending transfer event, halt, and IRQ state.
- LT target adds mutable CCI access delay. Controller transfer delay is also mutable and refreshed when scheduling.
- `transport_dbg` is intended to peek without FIFO/read-clear side effects, while debug writes use normal write semantics.
- The canonical AXI extension is included and extracted on main MMIO transactions, but no access decision depends on it.

## Existing test classification matrix

| Area | Existing coverage | Classification |
|---|---|---|
| Reset/basic CSRs | Reset image, status-empty bits, masks, RO/WI, WO/RAZ | Semantic, socket-driven |
| Controller commands | Write, read, repeated START, trailing-open segment, NACK, NAKOK, halt/resume | Semantic, socket-driven plus bus callback |
| FIFOs | RX/FMT reset, RX/FMT/TX/ACQ overflow and occupancy | Mixed: socket stimulus, direct debug occupancy oracle |
| Target behavior | Match/mismatch, target write/read, ACQ encoding, NACK count | Direct model methods bypass the TLM socket |
| Interrupts | Completion, halt, threshold levels, force, W1C, IRQ output | Semantic |
| Debug | Side-effect-free ACQ peek, debug write, broad `dbg_reg` branch sweep | Mostly coverage-oriented |
| TLM negative | Bad command, length 2/8, misalignment, aperture and decode holes | Semantic, incomplete protocol matrix |
| CCI | Presets, introspection, mutable access-delay handle, immutable sizing | Value checks; delay behavior not measured |
| Wrapper | Three CSR instances, masks, reset, one debug read | Semantic but incomplete TLM checks |
| Coverage/build | Release, separate ASan and coverage builds, aggregate source gate | Structural; contains runner weaknesses below |

## Evidence findings

### High

1. **Closed (2026-09-24) — ASan no longer ignores negative-bench exit.**  
   The runner combines primary and negative exits and fails if either is nonzero. SMC orchestrator ASan PASS. UBSan remains disabled (finding 2).

2. **The advertised sanitizer gate does not include UBSan.**  
   `CMakeLists.txt:41-45` uses only `-fsanitize=address`, despite the repository gate requiring AddressSanitizer plus UndefinedBehaviorSanitizer. Use the existing SMC option path but compile/link with `address,undefined` and ensure the runner recognizes UBSan diagnostics.

3. **The target-mode test interface bypasses the modeled bus protocol.**  
   Public methods at `i2c_controller.cpp:643-680` directly mutate ACQ/TX/event state; tests rely on them at `i2c_controller_tb.cpp:531-584,627` and `i2c_controller_neg_tb.cpp:202-210`. These are legitimate external-I2C stimulus adapters, but they cannot prove TLM ingress behavior, sideband propagation, timing, or that a platform binding invokes equivalent semantics. Keep adapter tests, but classify them separately from MMIO/TLM coverage and add an integrated bus-model/platform path.

4. **Main and wrapper TLM targets do not validate the full generic-payload contract.**  
   Main `b_transport` validates command/length/alignment/window only (`i2c_controller.cpp:564-608`); wrapper does the same (`:782-822`). Neither rejects null `data_ptr`, non-null byte enables, or invalid streaming width. Current negative tests (`i2c_controller_neg_tb.cpp:157-172`) omit these cases. This risks null dereference and silently accepting unsupported transfer shapes.

### Medium

5. **No canonical AXI sideband scenario is asserted.**  
   The extension is merely fetched and discarded (`i2c_controller.cpp:583-586`), and `i2c_wrap_ctrl` does not inspect it. No test attaches `smc::smc_axi_extension`, checks field integrity, or verifies behavior without an extension. Add attached/no-extension tests with all coherence setters and prove the upstream filter contract in integration.

6. **Annotated access delay is not tested.**  
   The driver discards the returned delay through its quantum keeper (`i2c_controller_tb.cpp:100-137`), while CCI testing only reads/mutates the handle (`:641-659`). Assert exact accumulation from a non-zero incoming delay before and after mutation, for both main and wrapper targets; assert errors do not add delay if that is the contract.

7. **Transfer-event scheduling and reset races are under-tested.**  
   `schedule_xfer` re-notifies one event (`i2c_controller.cpp:151-155`), reset cancels it (`:157-192`), and each FDATA write may reschedule (`:230-231`). Tests use broad 60 ns waits and do not cover reset immediately before expiry, changing `xfer_delay_ns` between queued segments, simultaneous queueing at expiry, host disable before the event, or repeated notifications.

8. **Read-length and callback result boundaries are incomplete.**  
   `drain_fmt` maps READB byte 0 to 256 and accepts callback `read_data` without clamping to requested length (`i2c_controller.cpp:321-327,346-350`). Tests cover lengths 3, 4, and 10 only. Add 0→256, 1, FIFO-depth, depth+1, callback-short, callback-exact, and callback-overlong results. The overlong case should establish the intended contract rather than silently enqueue arbitrary extra bytes.

9. **Target address matching is only tested for one exact primary address.**  
   Matching has two address/mask slots and treats mask zero as disabled (`i2c_controller.cpp:267-277`). Tests only use address0/mask0=0x50/0x7f. Cover masked aliases, near misses, address1, both enabled with overlap, both masks zero, 7-bit extremes, target disabled, and no-STOP target transfers.

10. **Several interrupt/event branches lack semantic tests.**  
    Missing or weak: every W1C source independently; W1C writes to level bits; ACQ_STRETCH abstraction; TX_STRETCH from each target event; TARGET_EVENTS W1C while IRQ enabled; FMT/RX/ACQ/TX thresholds at equality and one either side; saturation of `TARGET_NACK_COUNT`; error-bit clearing/reassertion; IRQ enable changed while status is already pending.

11. **`transport_dbg` semantics are inconsistent and under-specified.**  
    Debug reads of a decode hole return four zero bytes because `dbg_reg` defaults to zero (`i2c_controller.cpp:610-630,683-708`), while debug writes to the same hole return zero. Debug writes call normal `reg_write`, so they can push FIFOs, trigger transfers, clear events, and update IRQs. Existing tests call this “side-effect-free” while only proving the read side (`i2c_controller_tb.cpp:622-638`). Define and test debug write policy explicitly.

12. **Coverage-oriented direct-state assertions can hide externally visible defects.**  
    `dbg_*_count` is used repeatedly (`i2c_controller_tb.cpp:317,340,357,460-522,533,552,561`) where FIFO status registers can often provide an architectural oracle. The negative test loops over raw offsets specifically “for coverage of dbg_reg” (`i2c_controller_neg_tb.cpp:235-244`) without validating each register’s semantic value.

### Low

13. **Broad report suppression is present.**  
    `i2c_controller_tb.cpp:700-701` globally suppresses `SC_ID_LOGIC_X_TO_BOOL_`; the negative bench globally demotes `SC_ERROR` (`i2c_controller_neg_tb.cpp:126-129`). Narrow suppression to the exact expected probe and restore actions afterward so unrelated kernel/model errors remain fatal.

14. **Coverage reporting includes test sources, but the enforced gate covers only aggregate `src/`.**  
    `run_tests.sh:237-241` reports implementation, header, and tests, then `:352` invokes the shared gate. `enforce_line_coverage.sh:46-81` gates aggregate `.cpp` files under `src/` only. Inline hand-written header code and per-file regressions are not gated.

15. **No explicit DMI contract test.**  
    Main successful traffic sets `dmi_allowed=false` (`i2c_controller.cpp:607`), wrapper traffic does not. Add `get_direct_mem_ptr`/DMI-allowed checks for main and wrapper and verify stale incoming `dmi_allowed=true` is cleared on success and errors as required.

## Missing scenario inventory

### TLM2 and sideband

- Null pointer; zero length; byte-enable pointer with zero/non-zero length; streaming widths 0, 1, 3, 4, and >length.
- Exact last valid register, last aligned word in aperture, first hole, final aperture byte, and 64-bit address overflow.
- Read/write/IGNORE on main and wrapper; stale response and DMI fields; `transport_dbg` malformed command/pointer/BE/streaming cases.
- Canonical AXI extension attached with each source/privilege/security/fetch/lock field, absent extension, clone/copy behavior if payload reuse occurs.
- Exact annotated-delay delta and mutable CCI update; no simulated-time consumption in LT `b_transport`.

### Registers and FIFOs

- Every storage register reset/mask, especially TIMING1–4, TARGET_FIFO_CONFIG, TARGET_ID, HOST/TARGET_TIMEOUT, SMBUS status/control.
- Full FIFO status transitions empty→non-empty→full→not-full; all FIFO reset bits together and independently.
- Read-empty RDATA/ACQDATA; NEXT_DATA before and after pop; TX underflow target read; ACQ/TX overflow with START/STOP preservation.
- NACK counter 0xfe→0xff saturation, read-clear, software write masking, and NACK pulse at saturation.
- Wrapper offsets 0x0/0x4/0x8 plus 0x0c hole under every TLM error shape.

### Commands, timing, reset, events, interrupts

- Stray FDATA without START; START-only; STOP-only; repeated START after NACK; multiple segments where middle fails; read/write direction changes; RCONT semantics.
- No bus model, callback NACK with/without NAKOK, callback short/long reads, zero and 256-byte reads.
- Reset while transfer scheduled, halted, IRQ asserted, FIFOs full, and target transaction partly represented.
- START/STOP target transfers with `stop=false`; dual target ID/mask behavior.
- Every level and latched interrupt at boundary conditions, enable-after-pending, disable-while-asserted, W1C partial masks, force overwrite/clear behavior.

## Proposed test matrix

| ID | Layer | Scenario | Primary oracle |
|---|---|---|---|
| I2C-TLM-01 | Main MMIO | Full malformed payload matrix | Exact TLM response; no state/delay change |
| I2C-TLM-02 | Wrapper MMIO | Same matrix at 0x0c and 0x10 boundaries | Exact response and delay |
| I2C-TLM-03 | MMIO | AXI extension attached/absent | Extension fields preserved; upstream filter integration |
| I2C-TLM-04 | MMIO | DMI/debug policy | DMI denied; debug side effects match documented policy |
| I2C-REG-01 | Main MMIO | Table-driven reset/mask/access contract for every register | Independent expected constants from RDL |
| I2C-FIFO-01 | Main MMIO | Four FIFOs empty/full/overflow/reset boundaries | STATUS and FIFO_STATUS registers, IRQ bits |
| I2C-CMD-01 | Controller | Segment grammar and repeated START matrix | Captured bus transactions and response CSRs |
| I2C-CMD-02 | Controller | Read length 0/1/depth/depth+1/256; callback short/long | RX status/data and error semantics |
| I2C-TGT-01 | Target adapter + integration | Both ID/mask slots and stop/no-stop | ACQ architectural reads and events |
| I2C-IRQ-01 | MMIO/signals | Every interrupt: set, enable, IRQ, clear/reassert | INTR_STATE plus `irq_o` |
| I2C-TIME-01 | Kernel | Mutable access/xfer delay and event ordering | Exact timestamps/delay annotations |
| I2C-RST-01 | Kernel | Reset at scheduled-transfer and IRQ/FIFO states | No callback after reset; full reset image |
| I2C-BUILD-01 | Runner | Force each bench to return non-zero in ASan mode | **Closed 2026-09-24** — combined child exits |
| I2C-BUILD-02 | Sanitizer | Intentional UB fixture in test-only target | UBSan diagnostic causes failure |

## Verdict

**Needs material expansion before coverage can be considered protocol-complete.** The existing suite is strong on happy-path controller behavior, basic register semantics, and several FIFO/interrupt branches. The ASan runner swallowing negative-bench failures is **closed (2026-09-24)**. Remaining gaps: absent UBSan, incomplete generic-payload validation, no AXI sideband proof, unmeasured timing, and substantial dependence on direct target/debug methods. Line coverage near 99% is not a reliable proxy for these missing semantics.
