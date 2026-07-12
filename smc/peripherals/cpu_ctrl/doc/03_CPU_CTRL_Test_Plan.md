# SMC CPU Control — Test Plan

**Document**: `03_CPU_CTRL_Test_Plan.md`  
**Bench**: `test/cpu_ctrl_tb.cpp`  
**Pass criteria**: Prints `ALL TESTS PASSED`, exit code 0  

---

## Test matrix

| # | Test | Requirement covered |
|---|------|---------------------|
| 1 | Power-on defaults (`LOCAL_BASE`, `RESET_VECTOR`, `SCRATCH[*]=0`) | RDL reset values |
| 2 | SCRATCH R/W (absolute + relative addresses) | `SCRATCH[16]` storage |
| 3 | Inter-stage handoff sequence (indices 8,9,11,13,14,15) | `smc_rom.adoc` scratch contract |
| 4 | `LOCAL_BASE` write ignored | SW read-only |
| 5 | `GLOBAL_BASE` / `REGION_SIZE` R/W | Address-space CSRs |
| 6 | HW backdoor registers (`SMC_ATTRIBUTES`, `TEST_CTRL`, `WB_PC`) | HW=w / SW=r path |
| 7 | MUTEX acquire (read) and release (write) | `MUTEX[n]` semantics |
| 8 | SEMA signed inc/dec | `SEMA[n]` write delta |
| 9 | `WDT_TIMEOUT_RESET` self-clear | Single-pulse fields |
| 10 | `transport_dbg` mutex peek without acquire | Debug path |
| 11 | Out-of-window → `TLM_ADDRESS_ERROR_RESPONSE` | Window decode |
| 12 | CCI `access_delay_ns` preset | CCI broker integration |

---

## Build & run

```bash
cd smc/peripherals/cpu_ctrl
./run_tests.sh              # Release
./run_tests.sh --asan       # AddressSanitizer
./run_tests.sh --coverage   # Line coverage
./run_tests.sh --ctest      # Via CTest
```

From SMC root:

```bash
./run_all_smc_tests.sh cpu_ctrl
```

---

## Future integration tests

- Bind `cpu_ctrl.reg_socket` to `smc_fabric.to_cpu_ctrl` in `smc_fabric_tb`
  and verify scratch round-trip through MMIO at `0xC001_0140`.
- Dual-initiator SMC ROM writer + SEP reader threads exchanging scratch 9
  status bits under quantum keeper.
