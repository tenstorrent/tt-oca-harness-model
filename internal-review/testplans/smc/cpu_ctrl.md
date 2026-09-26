# SMC CPU Control Test Audit and Plan

## Files audited

- Model: `smc/peripherals/cpu_ctrl/include/cpu_ctrl.h`, `src/cpu_ctrl.cpp`
- Tests: `test/cpu_ctrl_tb.cpp`
- Build/coverage: `CMakeLists.txt`, `test/CMakeLists.txt`, `run_tests.sh`
- Shared coverage/ASan gates invoked by the runner.

## Behavior inventory

- CCI: base address and access delay; immutable watchdog stage-2 tick period.
- Ports/processes: register target; four watchdog sticky inputs; primary reset input; first/second timeout outputs; periodic tick, input-edge update, and sole output-driver methods.
- Registers: reset vectors/control/pulse count/timeout/reference counter; watchdog timeout/reset pulse; scratch mailbox; test control; per-core writeback PCs; attributes; mutexes; semaphores; dummy ROM words.
- Side effects: reset request self-clear, WDT reset pulse, mutex test-and-set on bus read/release on write, signed semaphore add, write-ignore RO fields.
- TLM: offset or absolute address normalization; 1/2/4/8-byte access; streaming/byte-enable/range/command checks; debug read/write; delay and DMI denial; AXI extension extraction.
- Hardware/debug APIs: attributes/test/WB-PC/scratch setters, register peek, dump, direct watchdog ticks.

## Existing scenario matrix

- **Genuine** — Major 32/64-bit register reset/read/write/mask/RO/RAZ-WI contracts, scratch handoff, mutex, semaphore, watchdog pulse, malformed TLM requests, and debug mutex peek are checked through `b_transport` (`cpu_ctrl_tb.cpp:229-565`).
- **Genuine** — Hardware-written attributes/test-control/WB-PC paths are stimulated through their explicit hardware APIs then observed over the bus (`cpu_ctrl_tb.cpp:306-315,444-488`).
- **Partial** — Watchdog stage 2 is advanced only by `dbg_wdt_stage2_tick`; the scheduled event process and configured nonzero period are never exercised (`cpu_ctrl_tb.cpp:334-357`).
- **Partial** — “Reset defaults” are constructor-state checks. The test never asserts `rst_primary_n_i` after modifying registers, and the model has no process that calls `reset_regs` on that input (`cpu_ctrl_tb.cpp:229-237`, `cpu_ctrl.cpp:113-121,577-582`).
- **Partial** — Scratch handoff status combines bit-position constants directly (`0|1|2 == 3`) rather than forming masks (`1<<bit`), so it does not represent all three named status bits (`cpu_ctrl.h:64-68`, `cpu_ctrl_tb.cpp:263-269`).
- **Coverage-only** — API bounds, dump formatting, gaps, and all dummy words are primarily line-completion cases (`cpu_ctrl_tb.cpp:456-537`).
- **Coverage-only** — Most access tests use only qword-base offsets, leaving the generic subword lane-merging implementation effectively untested.

No private-access macro, direct private decode call, or indiscriminate offset loop was found.

## Findings

- **Critical — nonzero-lane subword writes are implemented incorrectly and untested.** `reg_write` first inserts bytes into a full current qword, then passes that qword plus the same sub-offset to `write_qword` (`cpu_ctrl.cpp:368-378`). `write_qword` shifts `val` by `byte_off*8` again (`cpu_ctrl.cpp:237-245`). A 32-bit write at qword offset `+4`, for example, writes shifted old low-lane data rather than the caller's upper-lane value. Tests issue 32-bit accesses at qword bases and never cover +1/+2/+4 legal lanes.
- **High — null data pointer can crash.** Both transport paths copy through `buf` without validating it (`cpu_ctrl.cpp:435-487,492-513`).
- **High — periodic watchdog logic is untested.** The only test configuration leaves `wdt_stage2_tick_ns=0`, then calls the debug tick API; `wdt_stage2_tick_method` scheduling/rearm/reset branches receive no behavioral check (`cpu_ctrl.cpp:560-574`, `cpu_ctrl_tb.cpp:334-357`).
- **High — second-timeout aggregation can assert without a sticky first-stage timeout.** `any_second` is based solely on `count==0`, while a non-sticky channel reloads the count from `wdt_timeout_` (`cpu_ctrl.cpp:540-555`). If WDT_TIMEOUT is zero, every non-sticky channel yields second timeout. No zero-timeout test exists.
- **High — reset contract is missing from tests and likely incomplete in the model.** `rst_primary_n_i` only calls `wdt_stage2_step_once(false)`; it does not invoke `reset_regs` (`cpu_ctrl.cpp:577-582`). The test never proves whether primary reset must restore the register block.
- **Medium — range arithmetic is not overflow-safe.** `adr + len` and `off + len` checks can wrap (`cpu_ctrl.cpp:351,370,457,501`).
- **Medium — exact delay and mutable CCI behavior are not tested.** The test checks `access_delay_ns()==2.0` but never inspects the annotated delay (`cpu_ctrl_tb.cpp:373-376`).
- **Medium — AXI extension and DMI contracts are not tested.** The model extracts/discards the extension and sets DMI false (`cpu_ctrl.cpp:461-489`), but tests attach no extension and do not seed DMI true or request DMI.
- **Medium — mutex atomicity is tested with one initiator only.** No same-time two-initiator acquisition proves exactly one observes available.
- **Medium — semaphore edges are absent.** Only `+5,-2` is tested; 16-bit wrap, negative minimum, partial lanes, and upper-byte writes are not.
- **Medium — base-address mutability/aliasing is not tested.** Offset-only and absolute forms are accepted, while `base_addr_p_` is mutable. No test changes it during simulation or probes addresses around both windows.
- **Low — routine construction uses raw `SC_REPORT_INFO`.** This differs from the shared logging contract (`cpu_ctrl.cpp:126-135`) and has no logging-level test.

## Missing scenarios

- Every valid byte/halfword/word lane within each 64-bit register, especially offsets +1/+2/+4, with neighboring-byte preservation.
- Cross-register accesses rejected by natural alignment/size policy and upper-half writes to 32-bit fields.
- Real primary reset after mutating all register classes and while mutex/semaphore/WDT state is active.
- Automatic watchdog countdown, exact first/second timing, reload pulses per core, zero/one timeout, multiple sticky cores, reset during countdown, and output delta timing.
- Two-initiator mutex contention and release/acquire ordering.
- Semaphore overflow/underflow and all supported access widths.
- Null pointer, near-`UINT64_MAX` addresses, DMI, AXI extension, exact delay, error buffer/state preservation.
- Runtime base/access-delay CCI changes and address alias boundaries.
- Correct scratch status bit masks and virtual-console mailbox behavior.
- Debug write side effects for mutex, semaphore, pulses, RO fields, and malformed payloads.

## Proposed tests

1. **Subword lane oracle:** initialize a qword with `0x8877665544332211`, write unique 1/2/4-byte values at every naturally aligned lane, and expect a hard-coded resulting qword after each write. Exercise reset vector, scratch, dummy ROM, semaphore, and masked registers.
2. **Register reset matrix:** mutate every register class, acquire mutexes, alter semaphores, latch watchdog outputs, assert primary reset, and compare every documented post-reset value and output. If primary reset is not intended to reset registers, encode that explicitly.
3. **Automatic WDT:** instantiate tick period 10 ns; test timeout 0, 1, and 3 at `T-ε/T/T+ε`, per-core reload, multiple sticky inputs, deassertion, pulse reset, held primary reset, and resumed scheduling.
4. **Mutex contention:** bind two initiators and issue same-delta reads. Expect exactly one result=1 and one result=0; after release, exactly one can reacquire.
5. **Semaphore matrix:** 1/2/4/8-byte signed deltas, min/max values, wrap behavior, lane preservation, and independent instances.
6. **TLM malformed matrix:** null pointer, command, width, alignment, streaming, byte enables, window/overflow, and preseeded response/buffer/DMI. Expect exact response and no unintended side effects.
7. **Extension/DMI/timing:** attach every canonical AXI field and prove preservation; expect DMI denial; check exact additive delay before and after mutable CCI change.
8. **Address normalization:** probe offset, absolute base, just below/above each region, runtime base change, and overflow-prone addresses with identical expected register semantics only in the active window.
9. **Scratch handoff:** use `1u << CPU_CTRL_SEP_STATUS_*_BIT`, verify each bit independently and combined, and test all documented indices through bus-only producer/consumer accesses.
10. **Debug contract:** verify peeks are non-destructive, debug writes have documented side effects only, and malformed/unknown operations return zero.

## Verdict

**Critical register-access risk.** Broad base-lane coverage masks a concrete double-shift bug in legal subword writes. Reset semantics, real watchdog timing, zero-timeout behavior, mutex concurrency, and TLM pointer/extension/DMI/delay contracts also need dedicated tests.
