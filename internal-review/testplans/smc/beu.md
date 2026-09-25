# SMC BEU Test Audit and Plan

## Files audited

- Model: `smc/peripherals/beu/include/beu.h`, `src/beu.cpp`
- Tests: `test/beu_tb.cpp`, `test/beu_neg_tb.cpp`
- Build/coverage: `CMakeLists.txt`, `test/CMakeLists.txt`, `run_tests.sh`
- Shared coverage/ASan gates invoked by the runner.

## Behavior inventory

- CCI: mutable register access delay.
- Ports/processes: 64-bit target socket, active-low reset, local and PLIC IRQ outputs; reset and recompute methods.
- Registers: CAUSE, PHYS_ADDR, ENABLE, PLIC_ENABLE, ACCRUED_ENABLE, LOCAL_ENABLE with source mask `{1,2,5,6,7}`.
- Hardware event API: `inject_error` accrues a source and conditionally latches first enabled cause/address.
- IRQs: level functions of accrued status and independent local/PLIC masks.
- TLM: 8-byte accesses, address/decode errors, debug read/write, annotated delay, DMI denial, optional canonical AXI extension extraction.
- Debug: side-effect-free `dbg_reg` and state dump.

## Existing scenario matrix

- **Genuine** — Reset defaults, field masks, CAUSE width, PHYS_ADDR write-ignore, first-error latching, re-arm, recording gate, independent local/PLIC masks, per-source selectivity, and interrupt deassertion are asserted at registers and ports (`beu_tb.cpp:166-279`).
- **Genuine** — TLM command/length/alignment/window/decode responses and valid debug read/write are checked (`beu_neg_tb.cpp:109-152`).
- **Partial** — Error production necessarily uses the public hardware-event backdoor, but only legal enum values are supplied and event ordering/concurrency is not tested (`beu_tb.cpp:202-278`).
- **Partial** — CCI mutation checks the broker value but never the actual delay annotation (`beu_tb.cpp:303-319`).
- **Partial** — Reset is checked only at startup. The first `do_reset` writes the signal low while its default is already low, so constructor defaults—not necessarily `reset_proc`—can satisfy the assertions (`beu_tb.cpp:154-178`).
- **Coverage-only** — The negative bench explicitly exists to push line coverage and repeats already-covered masked injection/debug behavior (`beu_neg_tb.cpp:4-14,155-163`).
- **Coverage-only** — Dump checks only execute formatting paths; they do not validate model behavior (`beu_tb.cpp:322`, `beu_neg_tb.cpp:162`).

No private-access macro, direct private decode call, or indiscriminate offset sweep was found.

## Findings

- **Critical — ACCRUED_ENABLE contract is internally inconsistent and the test blesses the permissive implementation.** The header calls it “sticky accrued error status (HW-set, W-clear)” and says software clears relevant bits by writing zeroes (`beu.h:41-44,68-70`), but `reg_write` performs ordinary masked replacement, allowing software to set status (`beu.cpp:147-151`). The primary test explicitly requires software setting an unobserved error bit to raise IRQs (`beu_tb.cpp:259-264`). A write-clear register must be tested against an independent RDL-derived truth table.
- **High — null data pointers can crash.** Both callbacks copy through `buf` without a null check (`beu.cpp:168-202,215-230`); tests use only valid pointers.
- **High — byte enables and streaming width are ignored.** `b_transport` checks only command, length, and address (`beu.cpp:170-181`). Every test supplies width 8 and null byte enables.
- **Closed (2026-09-24) — ASan no longer suppresses negative-bench failure status.** The runner records `NEG_EXIT`/`COV_EXIT` and fails if any binary is nonzero. SMC orchestrator ASan PASS.
- **Medium — reset behavior is not proven after state mutation.** No test enables both IRQ paths, latches cause/address/accrued, asserts reset, and proves register/output clearing and ENABLE restoration.
- **Medium — invalid hardware source can invoke undefined behavior.** `inject_error` shifts `uint64_t{1}` by the enum's unchecked underlying value (`beu.cpp:239-246`). A casted invalid `beu_src` can shift by 64 or more.
- **Medium — delay and DMI contracts are unverified.** The model adds access delay and sets `dmi_allowed(false)` only after decoded operations (`beu.cpp:203-207`); tests neither measure delay nor seed/check DMI.
- **Medium — canonical AXI sideband is not covered.** The extension is fetched and discarded (`beu.cpp:183-187`); no transaction attaches one.
- **Medium — first-error priority is only temporal, not simultaneous.** Tests inject sequentially and do not define same-delta ordering among multiple source events.
- **Low — `transport_dbg` has no streaming-width, byte-enable, or null-pointer validation.** The existing edge matrix covers width/alignment/range/command only (`beu_neg_tb.cpp:137-152`).

## Missing scenarios

- Independent ACCRUED_ENABLE semantics: single-bit clear, multi-bit clear, writing ones/zeros to clear versus preserve, reserved bits, and prohibition on software-setting status.
- Reset after non-default register/state/output values, repeated reset, held-low reset, and reset coincident with injection.
- Every legal source individually across ENABLE, LOCAL_ENABLE, and PLIC_ENABLE; all five accumulated; first-latch ordering and address truncation to 56 bits.
- Invalid/casted source values without undefined shift.
- Exact delay, mutable CCI effect, DMI flag, `get_direct_mem_ptr`, canonical AXI extension.
- Null pointer, byte enables, streaming width mismatch, pre-existing response status, read-buffer preservation on errors.
- Debug accesses to every access class with explicit side-effect expectations.

## Proposed tests

1. **RDL-derived accrued truth table:** start with two injected bits and write each mask pattern. Expect exact sticky/W-clear behavior and IRQ changes. Never derive the expected value with the DUT's assignment expression.
2. **Post-mutation reset:** latch a cause/address, accrue all sources, modify all masks, assert both IRQs, then drive a real high-to-low reset edge. Expect CAUSE/PHYS/ACCRUED=0, ENABLE=`0xE6`, masks=0, and both outputs low.
3. **Source matrix:** for each legal source, test enabled/disabled recording and local/PLIC combinations. Expect exact cause enum, 56-bit address truncation, accrued bit, and IRQ levels.
4. **Priority/concurrency:** inject two sources in a defined same-delta sequence. Expect first event to own CAUSE/PHYS while both accrue; after CAUSE re-arm, expect the next new enabled event to latch.
5. **Malformed TLM matrix:** verify null pointer, byte enables, streaming width, wrong command/length/alignment/window/decode. Expect exact status, no state/output change, unchanged read buffer, and defined delay/DMI behavior.
6. **Extension/DMI:** attach a fully populated canonical extension and prove it remains unchanged. Seed `dmi_allowed=true`, expect false after supported transactions, and expect `get_direct_mem_ptr` denial.
7. **Timing:** seed a nonzero initial delay and mutate CCI between two requests. Expect exact additive delays.
8. **Debug contract:** valid debug read/write, PHYS write-ignore, unknown command, malformed payloads, and no IRQ/read side effects.

## Verdict

**High risk.** Core first-error and IRQ behavior has useful tests, but the accrued register's stated contract conflicts with both implementation and tests. Payload safety, TLM features, reset-after-use, timing, and AXI sideband remain inadequate. The ASan negative-bench exit hole is **closed (2026-09-24)**.
