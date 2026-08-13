# sep_cpu_ctrl SystemC TLM Model Test Plan

## 1. Test Plan Overview

This document provides the test plan for the `sep_cpu_ctrl` SystemC TLM model — the SEP CPU control/status register block (clock-gate control, boot straps/fuse-sense status, timeout counters/interrupts, NMI vector, EXT_TRNG source select, base-address/region-size windows, version ID). The existing suite (`sep_cpu_ctrl_test.cpp`) is a single smoke test (`sc_main` + one `SC_THREAD`), not a per-behavior test-function suite, so this plan documents exactly what that smoke test exercises. It does not invent additional rows for register fields or behaviors the current test does not touch — those gaps are listed explicitly in §5.2 instead.

### 1.1 Test Objectives

- Verify reset values for the register subset the test drives (`CLOCK_GATE_CTRL`, `SEP_VERSION_ID`, `SEP_LOCAL_BASE_ADDR`, `EXT_TRNG_SRC_SEL`, `nmi_vec_o`)
- Verify basic software read/write on a plain RW register (`CLOCK_GATE_CTRL`)
- Verify hardware-input passthrough registers (`TIMEOUT_INTERRUPT`) reflect `hwif_in` bits and ignore software writes
- Verify read-only registers (`SEP_VERSION_ID`) silently ignore software writes
- Verify `REFERENCE_COUNTER` reload-on-write, free-running semantics
- Verify the NMI-vector lock sequence: write-before-lock, `woset` lock-set, write-after-lock is ignored, lock cannot be cleared
- Verify the same lock pattern for `EXT_TRNG_SRC_SEL` / `EXT_TRNG_SRC_SEL_LOCK`
- Verify write-only-from-software registers (`TIMEOUT_CLEAR`, `TIMEOUT_MODE`) read back as 0
- Verify field masking on wide counter/address registers (`TIMEOUT_COUNT_DMA` 48-bit, `SEP_GLOBAL_BASE_ADDR` 56-bit)
- Verify `hwif_in`-driven boot-critical status (`SMC_FUSE_SENSE_STATUS`, `SEP_FUSE_SENSE_STATUS`)
- Verify `TIMEOUT_CLEAR` writes clear the targeted `hwif_in.*_timeout_int` bits selectively, and that clearing is a self-clearing (singlepulse) write
- Verify `SEP_TEST_CTRL` and `SEP_STRAPS` are live read-throughs of `hwif_in.fast_*_en`/`sep_standalone`/`test_en`/`bypass_mem_repair`

### 1.2 Test Environment

- SystemC TLM-2.0 testbench (`Tb` in `sep_cpu_ctrl_test.cpp`), a single `SC_MODULE` with one `SC_THREAD`
- `tlm_utils::simple_initiator_socket` (`isock`) bound directly to `dut.target_socket`; `do_read`/`do_write` are inline 8-byte `b_transport` helpers (no separate CSR-helper class, unlike the CSML pattern used by other peripherals' `*_test.h`/`*_basetest.h`)
- No reference model — expected values are hardcoded per assertion (masks/resets copied from the register map, `hwif_in` values set directly by the test before the corresponding read)

### 1.3 Port and Signal Names Reference

All test cases use exact port/signal names from `include/sep_cpu_ctrl.h`:

**Primary Ports:**
- `target_socket` — TLM target socket (inherited from `sep_cpu_ctrl_base`) for all CSR access
- `rst_ni` — active-low reset input; the test drives high→low→high pulses via `rst_n_sig`
- `nmi_vec_o` — `sc_out<uint32_t>`, driven from `SEP_NMI_VEC` register content (`fs_nmi_vec_ << 1`)

**`hwif_in` fields** (a plain `SepCpuCtrlHwifIn` struct member, not a TLM port — the test pokes it directly to emulate hardware-driven inputs the platform would otherwise wire up): `reference_counter_rc`, `sys_in_timeout_int`, `dma_data_timeout_int`, `alias_remap_timeout_int`, `filter_out_timeout_int`, `entropy_read_timeout_int`, `entropy_write_timeout_int`, `inbound_mailbox_timeout_int`, `outbound_mailbox_timeout_int`, `fast_spi_en`, `fast_iccm_en`, `fast_dccm_en`, `fast_sram_en`, `fast_pka_en`, `sep_standalone`, `smc_fuse_sense_done`, `sep_fuse_sense_done`, `test_en`, `bypass_mem_repair`.

## 2. Test Plan Table

Test cases are inline blocks inside `Tb::run()` (there are no discrete `test_*` functions); the "TestCase Name" column uses the exact `[PASS]` console-message string from the source as the identifier, grouped by the `T<n>` comment banner it falls under.

| Sl. No. | TestCase Name | Description | Registers Programmed | Ports/Signals Used | Test Type |
| ------- | ------------- | ----------- | --------------------- | ------------------- | --------- |
| **Reset Value Tests (T1)** |
| 1 | T1: CLOCK_GATE_CTRL reset values | Verify `pka_cg_enable`/`sram_cg_enable`/`cg_hysteresis` fields reset to `1`/`1`/`0x1F` | CLOCK_GATE_CTRL | `target_socket` | Positive |
| 2 | T1: SEP_VERSION_ID = 0xDEADBEEF | Verify the hardcoded version-ID register reads back `0xDEADBEEF` on reset | SEP_VERSION_ID | `target_socket` | Positive |
| 3 | T1: SEP_LOCAL_BASE_ADDR reset=0xC0000000 | Verify reset value of the local base-address window register | SEP_LOCAL_BASE_ADDR | `target_socket` | Positive |
| 4 | T1: EXT_TRNG_SRC_SEL reset=3 | Verify `EXT_TRNG_SRC_SEL` resets to `0x3` | EXT_TRNG_SRC_SEL | `target_socket` | Positive |
| 5 | T1: nmi_vec_o reset=0xC0000100 | Verify `nmi_vec_o` equals `0x60000080 << 1` on reset (derived from `SEP_NMI_VEC` reset value, not a register read) | SEP_NMI_VEC (indirectly) | `nmi_vec_o` | Positive |
| **Basic Read/Write Test (T2)** |
| 6 | T2: CLOCK_GATE_CTRL write/readback | Write `dma_cg_enable=1` and verify it reads back set | CLOCK_GATE_CTRL | `target_socket` | Positive |
| **Read-Only / HW-Input Passthrough Tests (T3)** |
| 7 | T3: REFERENCE_COUNTER reloads on SW write, then keeps counting | Seed `hwif_in.reference_counter_rc`, write `REFERENCE_COUNTER` to reload it, verify the next read has pre-incremented by one | REFERENCE_COUNTER | `target_socket`, `hwif_in.reference_counter_rc` | Positive |
| 8 | T3: TIMEOUT_INTERRUPT returns hwif_in bits | Set `hwif_in.sys_in_timeout_int`/`alias_remap_timeout_int`, write `TIMEOUT_INTERRUPT` (no effect — sw=r), verify the read reflects the `hwif_in` bits | TIMEOUT_INTERRUPT | `target_socket`, `hwif_in.sys_in_timeout_int`, `hwif_in.alias_remap_timeout_int` | Positive |
| 9 | T3: SEP_VERSION_ID ignores writes | Write a different value to `SEP_VERSION_ID` and verify it still reads back `0xDEADBEEF` | SEP_VERSION_ID | `target_socket` | Negative |
| **NMI Vector Lock Tests (T4)** |
| 10 | T4: SEP_NMI_VEC write before lock + nmi_vec_o driven | Write `SEP_NMI_VEC` before the lock is set; verify readback and that `nmi_vec_o` is updated accordingly | SEP_NMI_VEC | `target_socket`, `nmi_vec_o` | Positive |
| 11 | T4: SEP_NMI_VEC_LOCK set | Write `1` to `SEP_NMI_VEC_LOCK` and verify it reads back set | SEP_NMI_VEC_LOCK | `target_socket` | Positive |
| 12 | T4: SEP_NMI_VEC write silently ignored after lock | Attempt to overwrite `SEP_NMI_VEC` with all-ones after the lock is set; verify the value is unchanged | SEP_NMI_VEC, SEP_NMI_VEC_LOCK | `target_socket` | Negative |
| 13 | T4: SEP_NMI_VEC_LOCK cannot be cleared (woset) | Attempt to write `0` to `SEP_NMI_VEC_LOCK`; verify it remains set (write-once-set-only semantics) | SEP_NMI_VEC_LOCK | `target_socket` | Negative |
| **EXT_TRNG_SRC_SEL Lock Test (T5)** |
| 14 | T5: EXT_TRNG_SRC_SEL locked correctly | Program `EXT_TRNG_SRC_SEL`, set `EXT_TRNG_SRC_SEL_LOCK`, verify a subsequent write to `EXT_TRNG_SRC_SEL` is ignored | EXT_TRNG_SRC_SEL, EXT_TRNG_SRC_SEL_LOCK | `target_socket` | Negative |
| **Write-Only-from-Software Register Tests (T6, T7)** |
| 15 | T6: TIMEOUT_CLEAR reads back 0 | Verify `TIMEOUT_CLEAR` (a singlepulse, write-only-to-hardware register) always reads as `0` | TIMEOUT_CLEAR | `target_socket` | Positive |
| 16 | T7: TIMEOUT_MODE reads back 0 (write-only) | Write a non-zero value to `TIMEOUT_MODE` and verify it reads back `0` | TIMEOUT_MODE | `target_socket` | Positive |
| **Field Masking Tests (T8, T9)** |
| 17 | T8: TIMEOUT_COUNT_DMA 48-bit mask | Write all-ones and verify only the low 48 bits are retained (`0x0000FFFFFFFFFFFF`) | TIMEOUT_COUNT_DMA | `target_socket` | Positive |
| 18 | T9: SEP_GLOBAL_BASE_ADDR 56-bit mask | Write all-ones and verify only the low 56 bits are retained (`0x00FFFFFFFFFFFFFF`) | SEP_GLOBAL_BASE_ADDR | `target_socket` | Positive |
| **Fuse Sense Status Test (T10)** |
| 19 | T10: Fuse sense status reads from hwif_in | Set `hwif_in.smc_fuse_sense_done`/`sep_fuse_sense_done` and verify `SMC_FUSE_SENSE_STATUS`/`SEP_FUSE_SENSE_STATUS` read `1` | SMC_FUSE_SENSE_STATUS, SEP_FUSE_SENSE_STATUS | `target_socket`, `hwif_in.smc_fuse_sense_done`, `hwif_in.sep_fuse_sense_done` | Positive |
| **Timeout Interrupt Clear Test (T11)** |
| 20 | T11: TIMEOUT_CLEAR write clears targeted hwif_in bits | Assert all 8 `hwif_in.*_timeout_int` bits, verify `TIMEOUT_INTERRUPT` shows all set, clear a subset via `TIMEOUT_CLEAR` and verify only the targeted bits clear (others untouched), then clear the rest and verify `TIMEOUT_CLEAR` self-clears back to 0 on readback | TIMEOUT_INTERRUPT, TIMEOUT_CLEAR | `target_socket`, `hwif_in.*_timeout_int` (all 8) | Positive |
| **Test/Strap Control Read-Through Tests (T12, T13)** |
| 21 | T12: SEP_TEST_CTRL reflects hwif_in fast_*_en/sep_standalone | Set all 6 `hwif_in` fast-enable/standalone bits and verify `SEP_TEST_CTRL` bit positions 26-31 reflect them | SEP_TEST_CTRL | `target_socket`, `hwif_in.fast_spi_en`, `hwif_in.fast_iccm_en`, `hwif_in.fast_dccm_en`, `hwif_in.fast_sram_en`, `hwif_in.fast_pka_en`, `hwif_in.sep_standalone` | Positive |
| 22 | T13: SEP_STRAPS reflects hwif_in test_en/bypass_mem_repair | Set `hwif_in.test_en`/`bypass_mem_repair` and verify `SEP_STRAPS` bits `[1:0]` reflect them | SEP_STRAPS | `target_socket`, `hwif_in.test_en`, `hwif_in.bypass_mem_repair` | Positive |

## 3. Test Execution Strategy

### 3.1 Test Grouping

All 22 checks run sequentially inside a single `SC_THREAD` (`Tb::run()`) in one `sc_main` — there is no independent per-test setup/teardown or CTest-level test splitting:

1. **Reset Value Tests** (rows 1–5): power-on register/output defaults
2. **Basic Read/Write Test** (row 6): plain RW register sanity
3. **Read-Only / HW-Input Passthrough Tests** (rows 7–9): reload-on-write, hardware-input mirroring, RO write rejection
4. **NMI Vector Lock Tests** (rows 10–13): `woset` lock pattern
5. **EXT_TRNG_SRC_SEL Lock Test** (row 14): same lock pattern on a second register pair
6. **Write-Only-from-Software Tests** (rows 15–16): always-zero readback registers
7. **Field Masking Tests** (rows 17–18): wide-register bit-mask enforcement
8. **Fuse Sense Status Test** (row 19): boot-critical hardware-input status
9. **Timeout Interrupt Clear Test** (row 20): selective clear + self-clearing write
10. **Test/Strap Control Read-Through Tests** (rows 21–22): live `hwif_in` mirroring

### 3.2 Test Dependencies

- Row 12 depends on row 10 (NMI vector must be written before the lock test) and row 11 (lock must be set)
- Row 13 depends on row 11 (lock already set)
- Row 14's second write depends on its own preceding lock-set write within the same block
- Row 20's "clear the rest" step depends on the preceding "clear subset" step leaving the remaining bits set
- All rows execute against one persistent `dut` instance — later rows can observe register state left behind by earlier rows (e.g. `CLOCK_GATE_CTRL`'s `dma_cg_enable` from row 6 remains set for the rest of the run)

### 3.3 Pass/Fail Criteria

- **Reset values / plain RW**: register readback matches the documented reset value or the just-written value
- **HW-input passthrough**: register readback matches the `hwif_in` field(s) set immediately beforehand
- **Lock behavior**: pre-lock writes take effect; post-lock writes are silently ignored; the lock bit itself cannot be cleared by software
- **Field masking**: readback equals the written value AND-ed with the documented read/write mask
- **Interrupt clear**: only the bit(s) named in the `TIMEOUT_CLEAR` write clear; all others remain set; `TIMEOUT_CLEAR` itself always reads back 0

### 3.4 Test Coverage Metrics

- **Register Coverage**: 15 of the 30 registers in the map are exercised (`CLOCK_GATE_CTRL`, `REFERENCE_COUNTER`, `TIMEOUT_INTERRUPT`, `TIMEOUT_COUNT_DMA`, `TIMEOUT_CLEAR`, `TIMEOUT_MODE`, `SEP_TEST_CTRL`, `SEP_GLOBAL_BASE_ADDR`, `SEP_LOCAL_BASE_ADDR`, `SMC_FUSE_SENSE_STATUS`, `SEP_FUSE_SENSE_STATUS`, `SEP_STRAPS`, `SEP_NMI_VEC`, `SEP_NMI_VEC_LOCK`, `EXT_TRNG_SRC_SEL`, `EXT_TRNG_SRC_SEL_LOCK`, `SEP_VERSION_ID`) — see §5.2 for the untested remainder
- **hwif_in Coverage**: all fields in `SepCpuCtrlHwifIn` are exercised at least once
- **Lock-Pattern Coverage**: both lockable register pairs in the map (`SEP_NMI_VEC`/`_LOCK`, `EXT_TRNG_SRC_SEL`/`_LOCK`) are covered
- **Masking Coverage**: two of the several wide (48/56-bit) registers in the map are checked (`TIMEOUT_COUNT_DMA`, `SEP_GLOBAL_BASE_ADDR`); the other same-width timeout-count registers are not individually re-checked

## 4. Test Infrastructure Requirements

### 4.1 Testbench Components

- Single `SC_MODULE` (`Tb`) owning the DUT, one `tlm_utils::simple_initiator_socket`, an `rst_n_sig` signal, and an `nmi_vec_sig` output signal
- Inline `do_read`/`do_write` 8-byte `b_transport` helpers (no shared CSR-helper base class)
- Plain `assert()` checks plus `[PASS]` console lines — no `[FAIL]` path or pass/fail counter; a failed `assert()` aborts the process, so CTest pass/fail is the process exit code, not an accumulated tally
- `sc_main` calls `load_config_file()` (CCI ini loading) and uses `std::quick_exit(0)` on success specifically to avoid a CCI-broker-destructor crash on cleanup — this is a testbench-infrastructure detail, not DUT behavior

### 4.2 Reference Models

None — expected values are the register map's documented reset/mask constants (`sep_cpu_ctrl_basetest.h`'s `Register_Read_Access`/`Register_Write_Access`/`Register_Reset_Val` enums), copied inline into the assertions rather than computed from a shared table at test time.

## 5. Notes

### 5.1 Important Notes

- `hwif_in` is a plain struct the test (or, in the real platform, `och_sep_ss.hpp`) writes directly before/at any time during simulation — it is not a TLM port or `sc_signal`, so there is no timing/synchronization to verify beyond the one explicit `wait(sc_core::SC_ZERO_TIME)` needed after an `sc_out` write (T4) for the signal update phase to commit.
- The IP also exposes CCI-backed `csml_param<uint32_t>` mirrors of several `hwif_in` fields (`smc_fuse_sense_done`, `sep_fuse_sense_done`, `sep_standalone`, `fast_*_en`, `test_en`, `bypass_mem_repair`) for `.ini`-driven configuration in the VP. The smoke test does not exercise this path — it pokes `hwif_in` fields on the live object directly instead.

### 5.2 Important Exclusions

- `TIMEOUT_COUNT_SYS_IN`, `TIMEOUT_COUNT_MAILBOX_INBOUND`, `TIMEOUT_COUNT_MAILBOX_OUTBOUND`, `TIMEOUT_COUNT_ENTROPY_WRITE`, `TIMEOUT_COUNT_ENTROPY_READ`, `TIMEOUT_COUNT_FILTER_OUT`, `TIMEOUT_COUNT_ALIAS_REMAP` (only `TIMEOUT_COUNT_DMA` is checked, as the masking representative)
- `TIMEOUT_ENABLE`
- `SEP_REGION_SIZE`, `SMU_GLOBAL_BASE_ADDR`, `SMU_REGION_SIZE`
- `RAS_BANK_INFO`, `SEP_SW_DEBUG`, `KM_WIPE_CTRL`

Also excluded:
- **CCI/`csml_param` configuration path**: the `.ini`-driven strap/fuse parameters are not exercised (see §5.1).
- **Byte/halfword access granularity**: every `do_read`/`do_write` in the suite uses 8-byte accesses; narrower-width access to these registers is not tested.
- **Reset-pulse-during-active-lock interaction**: the single `rst_ni` pulse at the top of `run()` happens before any register is programmed; a reset asserted *after* a lock is set (to confirm the lock itself clears on reset) is not tested.
- **Cycle-accurate timing**: the model has no clock; all transactions complete with `SC_ZERO_TIME` delay.

### 5.3 Test Plan Alignment

This test plan aligns with:
- **Design doc**: `02_sep_cpu_ctrl_HighLevel_Design.md` — register map, lock semantics, `hwif_in`/`hwif_out`-style hardware interface convention
- **Source of truth**: `sep_cpu_ctrl_test.cpp` — every row above corresponds 1:1 to a `[PASS]` line in `Tb::run()`

## 6. Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 1.0 | 2026-08-05 | — | Initial test plan, derived from the existing 22-assertion-group `sep_cpu_ctrl_test.cpp` smoke test |
