# sep_scratch_warm SystemC TLM Model Test Plan

## 1. Test Plan Overview

`sep_scratch_warm` is the warm-domain counterpart to `sep_scratch_cold`: the same 8×64-bit `SCRATCH[0..7]` register layout (lower 32 bits `data`, RW, reset 0; upper 32 bits `Reserved0`, masked to 0), cleared by `rst_ni` via `reset_handler()`. Unlike `sep_scratch_cold`, it has **no write-callback side effects at all** — no VP-ack handshakes, no virtual-console/status decoders — it is purely a store (per `README.md`'s "Functional stubs" listing: warm-domain scratch, store-only stub). There is correspondingly very little functional behavior to test.

## 2. Current Test Status: implemented

`test/src/sep_scratch_warm_testbench.cpp` provides `sc_main` and the three cases below, wired
into CMake as the `sep_scratch_warm_testbench` target and registered with CTest, with a
`run_tests.sh` at the peripheral root matching every other IP. `test/src/sep_scratch_warm_test.cpp`
implements the `register_read_8`/`register_write_8` helpers that `test/inc/sep_scratch_warm_test.h`
had declared with no definition.

The target is compiled with `-UNDEBUG` so the `assert()` checks survive the default Release
build; without it `run_tests.sh` would report success regardless of behaviour.

## 3. Test plan

| Sl. No. | TestCase Name | Description | Registers Programmed | Ports/Signals Used | Test Type |
| ------- | ------------- | ----------- | --------------------- | ------------------- | --------- |
| 1 | `FUNC-SCRATCHWARM-001` Reset values | All 8 `SCRATCH` entries read 0 on a freshly-constructed DUT | SCRATCH[0-7] | `target_socket` | Positive |
| 2a | `FUNC-SCRATCHWARM-002a` Per-entry write/readback | A distinct pattern per entry, so a stride or aliasing error cannot pass | SCRATCH[0-7] | `target_socket` | Positive |
| 2b | `FUNC-SCRATCHWARM-002b` Reserved masking, 64-bit write | Write all-ones to one entry; `Reserved0[63:32]` must read 0 and the data half must keep the written value | SCRATCH[3] | `target_socket` | Positive |
| 2c | `FUNC-SCRATCHWARM-002c` Reserved masking, byte writes | Byte writes into the reserved half must leave it 0 *and* leave the data half untouched — the narrowest path in, and the one a masking bug is likeliest to survive | SCRATCH[4] | `target_socket` | Positive |
| 2d | `FUNC-SCRATCHWARM-002d` Byte access to the data half | Byte write/read round-trip, confirming partial access reaches the implemented bits at all | SCRATCH[5] | `target_socket` | Positive |
| 3a | `FUNC-SCRATCHWARM-003a` Entries hold programmed values | Precondition for 3b: all 8 entries hold a non-zero value before the pulse | SCRATCH[0-7] | `target_socket` | Positive |
| 3b | `FUNC-SCRATCHWARM-003b` rst_ni clears programmed state | Drive a real `rst_ni` pulse and verify all 8 read 0 — confirms `reset_handler()` is wired to the port, which a model that merely initialised its storage would fail | SCRATCH[0-7] | `target_socket`, `rst_ni` | Positive |

No VP-ack, decoder, or CCI-parameter rows apply here — this model has none of those (that is the entire difference from `sep_scratch_cold`).

## 4. Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 1.0 | 2026-08-05 | — | Initial test plan: documents that no test executable currently exists, and proposes a minimal 3-case plan for when one is added |
| 1.1 | 2026-08-13 | — | Testbench, CMake target and `run_tests.sh` implemented; plan updated from proposal to as-built |
