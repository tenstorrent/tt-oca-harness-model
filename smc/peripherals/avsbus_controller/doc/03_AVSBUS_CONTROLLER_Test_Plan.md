# AVSBus Controller — Test Plan

## Primary bench (`avsbus_controller_tb`)

| # | Case | Checks |
|---|------|--------|
| 1 | Reset values | CFG/CONFIG/MASK/FIFOS/idle status, IRQ low, GPIO enable |
| 2 | Register RW/RO | CFG_0, MASK, CONFIG↔gpio, WO RAZ, RO WI |
| 3 | Cmd/response | Push write cmd → delayed response → peek vs pop |
| 4 | IRQ mask/clear | HAS_DATA, inject SLAVE_ISSUED, mask disables pin |
| 5 | Cmd overflow | 8 fills + 9th dropped + OVERFLOW irq |
| 6 | Rb full | After burst of 8 responses |
| 7 | Rb underflow | Pop empty → UNDERFLOW irq |
| 8 | Max retries | Slave always BAD_CRC → MAX_RETRIES |
| 9 | Forced resync | CFG_1 bit9 → status bit22 pulse |
| 10 | Read voltage | Canned 0x03E8 + slave status |
| 11 | CCI / dbg | Param handle, `dbg_reg`, `dump_state` |

## Negative bench (`avsbus_controller_neg_tb`)

| # | Case |
|---|------|
| 0 | Constructor FATAL on zero FIFO depth |
| 1 | TLM BURST / ADDRESS / COMMAND / decode-miss (read+write) |
| 2 | `transport_dbg` read/write/IGNORE + full `dbg_reg` switch |
| 3 | RO write-ignore + WO RAZ (`AVS_CMD`, `AVS_INTERRUPT_CLEAR`) |
| 4 | Readback backpressure (`complete_one_xfer` early-out when full) |
| 5 | `inject_readback` → `READBACK_OVERFLOW` + retry counter |
| 6 | `ACK_BAD_DATA` no-retry + `ACK_BUSY` then OK |
| 7 | Default canned READ responses for all cmd codes + HAS_DATA re-assert |
| 8 | Force resync while a transfer is pending |
| 9 | Field helpers / OOR `inject_interrupt` / `dump_state` |
| 10 | `crc3(0) == 0` |
| 11 | Empty debug peek returns `0xDEADBEEF` |

## Pass criteria

Both binaries print `ALL TESTS PASSED` and exit 0. Wired into
`smc/run_all_smc_tests.sh` as `avsbus_controller`.
