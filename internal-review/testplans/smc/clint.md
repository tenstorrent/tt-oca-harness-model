# SMC CLINT Test Audit and Plan

## Files audited

- Model: `smc/peripherals/clint/include/clint.h`, `src/clint.cpp`
- Tests: `test/clint_tb.cpp`, `test/clint_tick_tb.cpp`
- Build/coverage: `CMakeLists.txt`, `test/CMakeLists.txt`, `run_tests.sh`
- Shared coverage/ASan gates invoked by the runner.

## Behavior inventory

- CCI: immutable hart count and tick period; mutable access delay.
- Ports/processes: target socket, per-hart MSIP/MTIP vectors, active-low reset; reset, periodic tick, and sole output-driver methods.
- Registers: 32-bit MSIP array; 32/64-bit MTIMECMP array; 32/64-bit MTIME; in-window holes RAZ/WI.
- Interrupts: `MSIP=bit0`; `MTIP=(MTIME>=MTIMECMP)`.
- TLM: width/alignment/streaming/byte-enable/range/command checks, split-lane accesses, annotated delay, debug read/write.
- Debug: MTIME/MTIMECMP/MSIP/MTIP peeks, direct MTIME set, dump.

## Existing scenario matrix

- **Genuine** — MSIP masks/isolation, 32/64-bit MTIME and MTIMECMP, firmware update/read sequences, comparator boundaries, per-hart independence, interrupt independence, holes, and malformed `b_transport` requests are checked through the socket and ports (`clint_tb.cpp:320-699`).
- **Genuine** — A separate real-time bench covers constructor tick scheduling, reset cancel/rearm, automatic MTIME increments, and MTIP generation (`clint_tick_tb.cpp:132-199`).
- **Partial** — Most timer behavior uses `dbg_set_mtime`, bypassing the tick event/process; the separate tick bench covers only one hart and one comparator (`clint_tb.cpp:303-309,491-513`).
- **Partial** — CCI delay mutation verifies only the parameter handle; no transaction's exact annotated delay is asserted (`clint_tb.cpp:574-611`).
- **Partial** — Tick assertions use broad lower bounds (`>=9`, `>=1`, `>=cmp`) despite deterministic SystemC scheduling, so duplicate/early ticks can escape (`clint_tick_tb.cpp:150-190`).
- **Coverage-only** — Tests 18-22 are explicitly line-oriented (“covers lines...”) for half reads, holes, and malformed/debug branches (`clint_tb.cpp:627-726`). They have checks, but scenario selection is coverage-driven.
- **Coverage-only** — Dump string substring checks add no timer/interrupt confidence (`clint_tb.cpp:614-624`).

No private-access macro or direct private decode call was found.

## Findings

- **Critical — long reset can permanently stop automatic ticking.** Reset cancels and schedules a tick immediately from assertion (`clint.cpp:145-158`). If reset remains low for at least one period, `tick_method` returns without rearming (`clint.cpp:173-182`); deassertion does nothing, so MTIME never resumes. The tick test holds reset low only 10 ns with a 50 ns period (`clint_tick_tb.cpp:123-129,216-225`).
- **High — constructor ignores `cfg.access_delay_ns`.** `access_delay_ns_p_` is initialized with hard-coded `2.0` rather than `cfg.access_delay_ns` (`clint.cpp:57-61`). Existing tests always preset the parameter, hiding constructor-default misuse.
- **High — null data pointer can crash.** Production and debug callbacks copy through `buf` without validation (`clint.cpp:367-413,437-459`).
- **High — `transport_dbg` treats every non-read command as a write.** Its `else` branch does not require `gp.is_write()` (`clint.cpp:449-459`), so IGNORE can mutate MTIME/registers. Existing debug tests omit unknown command.
- **High — normal and ASan runners omit the real tick bench.** `clint_tick_tb` runs only during coverage or `--ctest`; default and ASan execute only `clint_tb`, which disables ticking (`run_tests.sh:170-247,249-332,400-405`).
- **Medium — DMI is not explicitly denied.** Unlike peer models, successful `b_transport` never calls `set_dmi_allowed(false)` (`clint.cpp:424-430`), despite the header promising DMI is never granted (`clint.h:203-208`).
- **Medium — reset races and held-reset writes are undefined by tests.** No request is issued while reset is asserted, near a tick boundary, or while MTIP/MSIP is high.
- **Medium — canonical AXI extension is included/documented but not extracted.** No model line accesses it and no test attaches it.
- **Medium — rollover/atomicity is mostly synthetic.** Firmware high/low/high read is tested with a static MTIME set by backdoor; no tick occurs between split reads, so rollover retry behavior is not exercised (`clint_tb.cpp:389-394`).
- **Medium — invalid debug index paths are only API peeks.** Out-of-range hart getters return zero/false (`clint.cpp:468-482`) but are not tested.
- **Low — global `SC_ERROR` demotion can hide unrelated reports.** The primary test suppresses errors for immutable CCI mutation (`clint_tb.cpp:740-747`).

## Missing scenarios

- Reset held for less than, equal to, and greater than one tick period; exact restart phase after deassertion.
- Reset simultaneous with pending tick, MTIP assertion, split comparator update, and bus request.
- Exact MTIME sequence and tick count, including wrap from `UINT64_MAX` to zero.
- Real split-read rollover with a tick between high/low/high reads.
- MTIMECMP split-write transient behavior and firmware sequence guarantee under active ticking.
- Null pointer; debug unknown command; debug streaming/byte-enable policy; DMI flag/callback.
- Exact access delay using constructor config and runtime CCI mutation.
- Canonical AXI extension preservation.
- Hart count boundaries 1 and 4095 plus invalid 0/4096 fatal report identity.
- 64-bit MSIP attempts on read and write, all invalid half-address/size combinations, and highest legal window address.

## Proposed tests

1. **Long-reset timer restart:** hold reset low for 3.5 periods. Expect MTIME=0 and outputs low throughout; after deassertion expect first tick at the documented phase and exact subsequent increments.
2. **Constructor config delay:** instantiate without a preset using `cfg.access_delay_ns=7`. Seed delay 5 ns and expect 12 ns after a valid access. Mutate CCI to 3 and expect 8 ns next time.
3. **Debug command safety:** send READ, WRITE, and IGNORE to MTIME/MSIP. Expect IGNORE to return zero and leave state unchanged; malformed/null requests return zero without crash.
4. **DMI/extension:** seed DMI true and attach all canonical AXI fields. Expect DMI false, extension unchanged, and `get_direct_mem_ptr` denied.
5. **Live rollover:** set MTIME near `0xFFFF'FFFF`, enable ticking, deliberately place a tick between low/high reads, and verify the firmware high/low/high loop returns a coherent value.
6. **Comparator update under ticks:** exercise unsafe low/high order and firmware high=max/low/high order. Check every MTIP transition at delta-cycle precision.
7. **Reset race matrix:** assert reset one delta before, at, and after a scheduled tick and while interrupts are high. Expect exact state/output and no stale event.
8. **Payload matrix:** null pointer, byte-enable combinations, streaming mismatch, width/alignment/window/command, read-buffer preservation, and error delay policy.
9. **Topology boundaries:** valid 1 and representative multi-hart instances plus invalid bounds; verify vector sizes, last-hart decode, and exact fatal message.
10. **Runner regression:** include `clint_tick_tb` in Release and ASan phases and propagate its exit status.

## Verdict

**Critical timer risk.** Register and comparator coverage is otherwise strong, but a held reset can stop the timer permanently. The suite also hides constructor-delay misuse, permits unsafe debug command handling, lacks pointer/DMI/AXI checks, and excludes the only real tick test from normal and ASan runs.
