# AOU_CORE — Test Plan

## 1. Unit bench (`aou/test/aou_core_tb.cpp`)

Dual `aou_core` with `connect_peer`, FDI tied high, peer memory on B.

| # | Case | Checks |
|---|------|--------|
| 1 | CSR reset | `ip_version`, `aou_con0`, DISABLED in `aou_init` |
| 2 | APB decode errors | Bad length/alignment → `TLM_GENERIC_ERROR_RESPONSE`; offset ≥ `kWindowSize` → `TLM_ADDRESS_ERROR_RESPONSE` |
| 3 | Named CSR round trip | All-ones write/read-back on `aou_interrupt_mask`, `lp_linkreset`, `prior_rp_axi`, `prior_timer` matches each register's write mask |
| 4 | Per-RP CSR bank | Read/write at `OFF_RP0_BASE` (array-indexed decode path, outside every named offset) |
| 5 | AXI deny | Write while DISABLED → `TLM_COMMAND_ERROR_RESPONSE` |
| 6 | Activate | Local `activate_start` → both ENABLED; IRQ then W1C |
| 7 | Soft reset while enabled | `aou_con0.aou_sw_reset` → all CSRs + DISABLED on A only; B unaffected |
| 8 | AXI loopback | A `axi_s[0]` write/read appears in B memory |
| 9 | AXI bridge dest_rp out-of-range | `dest_rp` pointing past peer `rp_count` → `TLM_ADDRESS_ERROR_RESPONSE` |
| 10 | Deactivate | `deactivate_start` → DISABLED |
| 11 | `aou_con0` WO paths | `ip_version` write is a no-op; `deactivate_force` (bit 0) forces ENABLED → DISABLED unconditionally |

Pass: prints `ALL TESTS PASSED`, exit 0.

```bash
cd aou && ./run_tests.sh
./run_tests.sh --coverage   # ≥ 95% on aou_core.cpp (currently 100%)
```

## 2. Platform smoke (`sw/smc-vp-tests/smc-aou-test`)

Hart-0 MMIO through `smc-vp` at `SMC_AOU_BASE` (`0xC000_E000`):

| # | Case |
|---|------|
| 1 | Reset values (`ip_version`, `aou_con0`, `dest_rp`, DISABLED) |
| 2 | Activate → ENABLED + `int_activate_start`; W1C clear |
| 3 | Deactivate → DISABLED |

```bash
cd sw/smc-vp-tests && ./run_smc_vp_tests.sh smc-aou-test
```

## 3. Pass criteria

- Unit: `ALL TESTS PASSED`
- Coverage: `./run_tests.sh --coverage` ≥ 95% line on touched sources
- ASan: `./run_tests.sh --asan` when `libasan` is available
- VP: firmware test prints `ALL TESTS PASSED` under `smc-vp`
