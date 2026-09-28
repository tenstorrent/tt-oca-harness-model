# sep_cpu_ctrl Test Audit and Plan

## Audited files

- Model: `include/sep_cpu_ctrl.h`, `sep_cpu_ctrl_base.h`, `sep_cpu_ctrl_register.h`, `src/sep_cpu_ctrl.cpp`, `src/sep_cpu_ctrl_base.cpp`.
- Tests: `test/sep_cpu_ctrl_test.cpp`, `test/inc/sep_cpu_ctrl_basetest.h`, `test/src/sep_cpu_ctrl_basetest.cpp`.
- Build/coverage: `CMakeLists.txt`, `run_tests.sh`.

## Behavior inventory

- One 64-bit CSR target exposes 36 control/status registers through offset `0x1000`.
- Plain register masks cover clock/PKA controls, eight 48-bit timeout counters, timeout enable/mode, address windows, RAS/debug, wipe, and status registers.
- Reference counter is a free-running thread with mutable CCI period, reset-to-zero, and software reload callback.
- Hardware inputs synthesize timeout status, test controls, fuse status, and straps.
- Timeout clear is a targeted single-pulse clear.
- NMI vector and external TRNG selection have lock-gated data plus sticky WOSET locks; NMI drives an output.
- Global base/region size post-write hooks publish output ports one delta later; a public republish API supports platform backdoor seeding.
- DMA/peripheral bus-error clear registers clear all or selected status bits and self-clear.
- End-of-elaboration applies CCI strap/fuse values and reset NMI output.
- Active-low reset clears registers, counter, locks, output vector, and republishes inbound window.

## Existing tests matrix

### Genuine

- CSR helpers use the real target socket and assert `TLM_OK_RESPONSE`.
- Release explicitly uses `-UNDEBUG`, preserving all assertions (`CMakeLists.txt:158-163`).
- Tests cover representative reset values, masks, normal RW/RO behavior, reference counter reload/count timing, timeout read/clear, NMI/TRNG locks, fuse/test/strap hardware inputs, inbound-window output delta timing, and reset output behavior.
- The test drives a real reset edge and checks externally visible signals.

### Partial

- Reset checks only a subset of 36 registers.
- Most plain registers are represented only in the unused `reg_map`; there is no table-driven executable mask/reset/access test.
- Hardware-driven behavior is stimulated by directly mutating the public `hwif_in` struct. This is acceptable as a hardware-input API, but it does not test temporal synchronization or port wiring.
- CCI defaults/overrides are not exercised; only direct `hwif_in` mutations are checked.
- The reference counter checks one period and one reload but not zero/negative period, runtime period changes, rollover, or reset races.

### Coverage-only

- Inbound-window backdoor test directly assigns public register objects and calls `publish_inbound_window()` (`test/sep_cpu_ctrl_test.cpp:336-348`). It verifies the explicit platform backdoor API but bypasses the bus/post-write path.
- Bus-error status tests directly poke `DMA_BUS_ERR_STATUS` and `PERIPH_BUS_ERR_STATUS` (`test/sep_cpu_ctrl_test.cpp:360-363`) because no hardware input API exists. This covers clear callbacks without proving realistic status-set behavior.
- `sc_main` always calls `quick_exit(0)` (`test/sep_cpu_ctrl_test.cpp:381-391`). Assertion failures abort, but any non-assert SystemC error/report or future counted failure cannot affect exit status.
- No private-public macro or direct private callback call was found.

## Shortcut findings

- **High — direct status/register pokes:** bus-error and inbound-window paths bypass sockets/hardware interfaces (`test/sep_cpu_ctrl_test.cpp:336-363`).
- **High — unconditional success exit:** `quick_exit(0)` (`test/sep_cpu_ctrl_test.cpp:390`) is unsafe for future non-assert failures and can hide report-only failures.
- **Medium — broad register map, narrow executable coverage:** 36 entries are declared (`test/src/sep_cpu_ctrl_basetest.cpp:3-41`), but only a subset is semantically tested.
- **Medium — TLM helpers assume every payload is valid:** no negative response or malformed payload test exists.
- **Medium — coverage excludes the hand-written register header as “generated”:** `CMakeLists.txt:187-196` conflicts with this repository's hand-written-register policy and omits mask/type code from coverage.
- **Low — `SC_MANY_WRITERS` masks accidental NMI drivers:** test signal uses many-writer policy (`test/sep_cpu_ctrl_test.cpp:29-31`) even though the architecture intends one output driver.
- No expected-algorithm clone, disabled report policy, or offset-touch loop was found.

## Missing scenarios

- Every register reset, read mask, write mask, reserved bits, RO/WO/RW semantics, first/last mapped offsets, and aperture holes.
- TLM2 malformed command/address/length/pointer, byte enable, streaming width, unaligned accesses, debug/DMI, and delay.
- NMI/TRNG lock boundaries: reserved bits, repeated set, reset unlock, simultaneous data+lock writes, bit-zero NMI alignment.
- Reference counter: period CCI preset, runtime change, `period<=0`, reset while waiting, deassert resumption, 64-bit rollover.
- CCI integration for all ten straps/fuses and period; broker preset before construction and metadata/introspection.
- Window publish: separate writes, repeated same-value writes, reset after reprogram, backdoor call ordering, signal stability.
- Bus errors: realistic hardware-set path, partial clear combinations, zero write, reserved bits, reset.
- Timeout flags: all eight one at a time and together, selective clear, clear zero/reserved, new assertion after clear.
- No interrupt output or DMA engine belongs to this controller; timeout status is polled CSR state only.

## Proposed cases

1. **Full register contract sweep:** for all 36 registers, reset then socket-read exact value, write all ones/alternating patterns, and assert independent literal read/write masks. Treat side-effect registers separately.
2. **Raw TLM negative matrix:** issue IGNORE, unmapped and unaligned addresses, lengths 0/1/4/8/9, null data, BE patterns, and streaming widths; assert exact response and no state change.
3. **Lock sequence matrix:** for NMI and TRNG, test write-before-lock, lock=0, lock=1, repeated lock, post-lock writes, reset unlock, reserved masks, and output/readback equality.
4. **Reference-counter temporal suite:** use CCI presets for 1 ns and disabled periods; assert exact counts across periods, runtime period update, reset mid-wait, and rollover from `UINT64_MAX`.
5. **Timeout matrix:** drive each `hwif_in` timeout independently, read exact bit, clear exact bit, prove neighbors persist, and prove reassertion works.
6. **Hardware status injection API:** add/use a legitimate hardware-facing method or port in the test environment for DMA/peripheral bus error set behavior; then test clear through CSR without direct register assignment.
7. **Window export lifecycle:** check time-zero reset exports, each CSR independently, reserved masking, reset restore, same-value notification behavior, and documented backdoor republish.
8. **CCI construction test:** install broker presets before DUT construction and assert all fuse/strap/test-control reads plus invalid/edge period values.
9. **Fail-closed harness:** replace unconditional `quick_exit(0)` with tracked failures and configure unexpected `SC_ERROR` to fail.
10. **Coverage scope correction:** include hand-written `sep_cpu_ctrl_register.h` instantiated code or document a reviewer-approved exclusion; report per-file line coverage.

## Verdict

The implemented special callbacks have useful focused checks, and assertions remain active in Release. Overall confidence is **medium-low** because most registers are untested, TLM negatives are absent, key hardware status paths are covered by direct state pokes, coverage excludes a hand-written register header, and the harness exits success unconditionally.
