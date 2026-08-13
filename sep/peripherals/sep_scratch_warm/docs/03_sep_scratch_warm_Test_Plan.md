# sep_scratch_warm SystemC TLM Model Test Plan

## 1. Test Plan Overview

`sep_scratch_warm` is the warm-domain counterpart to `sep_scratch_cold`: the same 8×64-bit `SCRATCH[0..7]` register layout (lower 32 bits `data`, RW, reset 0; upper 32 bits `Reserved0`, masked to 0), cleared by `rst_ni` via `reset_handler()`. Unlike `sep_scratch_cold`, it has **no write-callback side effects at all** — no VP-ack handshakes, no virtual-console/status decoders — it is purely a store (per `README.md`'s "Functional stubs" listing: warm-domain scratch, store-only stub). There is correspondingly very little functional behavior to test.

## 2. Current Test Status: no test executable exists

Unlike every other peripheral in `sep/peripherals/`, `sep_scratch_warm` has **no runnable test today**:

- `test/inc/sep_scratch_warm_basetest.h` / `test/src/sep_scratch_warm_basetest.cpp` — present, but contain only register-map metadata (`Register_offset`/`_Read_Access`/`_Write_Access`/`_Reset_Val` enums for the single `SCRATCH` offset, reused from a template shared with other IPs) and an unbound `initiator_socket`.
- `test/inc/sep_scratch_warm_test.h` — declares `register_read_8`/`register_write_8`, but **no `.cpp` implements them** — there is no `sep_scratch_warm_test.cpp` in the tree.
- There is no testbench file, no `sc_main`, and no CMake test target: `CMakeLists.txt` builds `sep_scratch_warm_model` only and never `add_subdirectory(test)` or registers anything with CTest.
- There is no `run_tests.sh` at the peripheral root (every other peripheral has one).

This means `sep_scratch_warm` is not exercised by `run_all_peripherals.sh`, has no coverage number, and cannot pass `.cursor/rules/new-ip-ci-checklist.mdc`'s "every peripheral needs a `test/` dir + `run_tests.sh`" requirement as it stands. This is a gap, not a design choice — worth flagging for whoever picks this up next, separately from the small size of the model itself.

## 3. Recommended minimal test plan (not yet implemented)

If/when a testbench is written, given how little behavior exists to verify, three cases (mirroring `sep_scratch_cold`'s `FUNC-SCRATCH-001`/`002`/`004` and `local_master_alias_remap_ctrl`'s reset-pulse test) would fully cover the model:

| Sl. No. | TestCase Name | Description | Registers Programmed | Ports/Signals Used | Test Type |
| ------- | ------------- | ----------- | --------------------- | ------------------- | --------- |
| 1 | Reset values | Verify all 8 `SCRATCH` registers' lower 32 bits read 0 on a freshly-constructed DUT | SCRATCH[0-7] | `target_socket` | Positive |
| 2 | Basic read/write + reserved-bit masking | Write a distinct pattern to each register's lower 32 bits and verify readback; write `0xFFFFFFFF` to one register's upper 32 bits and verify it (and the lower half) still read 0 | SCRATCH[0-7] | `target_socket` | Positive |
| 3 | rst_ni clears programmed state | Program non-zero values into all 8 registers, drive a real `rst_ni` pulse (assert/deassert), and verify all 8 read back 0 afterward — confirms `reset_handler()` is actually wired, not just relying on the freshly-constructed default from case 1 | SCRATCH[0-7] | `target_socket`, `rst_ni` | Positive |

No VP-ack, decoder, or CCI-parameter rows apply here — this model has none of those (that is the entire difference from `sep_scratch_cold`).

## 4. Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 1.0 | 2026-08-05 | — | Initial test plan: documents that no test executable currently exists, and proposes a minimal 3-case plan for when one is added |
