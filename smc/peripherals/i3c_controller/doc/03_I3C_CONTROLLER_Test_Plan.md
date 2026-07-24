# OCA I3C Controller — Test Plan

**Style:** SystemC / TLM-2.0 LT · self-checking benches · CCI 1.0

This plan enumerates the verification intent for the OCA I3C Controller model.
Two self-checking benches implement it; each prints `ALL TESTS PASSED` on
success and exits non-zero on any failure.  They are registered with CTest and
run by `run_tests.sh`.

---

## 1. Strategy

- **Primary bench** (`i3c_controller_tb.cpp`): functional coverage against a
  running kernel — register access, multi-instance decode, device tables, the
  full HCI transaction flow (write/read/CCC), error paths, IBI, interrupt
  aggregation, queue resets, and CCI introspection.  Uses a 3-instance DUT and
  attaches a bus model emulating a single target at dynamic address `0x42`.
- **Negative bench** (`i3c_controller_neg_tb.cpp`): construction guard rails
  and TLM/`transport_dbg` error paths, exercised during elaboration without
  `sc_start` (so constructor-throw probes do not perturb the kernel).
- **Instrumentation**: `--asan` (AddressSanitizer, + LSan on Linux) must report
  zero errors; `--coverage` produces a line-coverage report.

---

## 2. Primary bench cases

| # | Area | Intent |
|---|------|--------|
| 1 | Reset values | `HCI_VERSION=0x120`, `HC_CONTROL=0x40` (MODE_SELECTOR=1), `QUEUE_THLD_CTRL`/`DATA_BUFFER_THLD_CTRL=0x01010101`, `PIO_CONTROL=0x03`, `PIO_SECTION_OFFSET=0x80`, `STBY_CR_EXTCAP_HEADER=0x12`, `DAT_SECTION_OFFSET` fields. |
| 2 | RW / RO / W1C | RW round-trip (`CONTROLLER_DEVICE_ADDR`); RO ignore (`HCI_VERSION`); `INTR_FORCE` set + `INTR_STATUS` W1C clear. |
| 3 | DAT / DCT windows | Write/read-back of DAT and DCT direct-access entries. |
| 4 | Bus enable | `HC_CONTROL.BUS_ENABLE`; `PRESENT_STATE.AC_CURRENT_OWN` follows. |
| 5 | Private write | TX push + command → response (TID/len/SUCCESS); bus model receives little-endian bytes. |
| 6 | Private read | Read command → response + RX payload via `XFER_DATA_PORT`. |
| 7 | CCC write | `CP=1`, CCC code 0x80 → response SUCCESS; payload delivered. |
| 8 | Address NACK | Command to an unpopulated DAT index → `ERROR_ADDRESS_NACK`. |
| 9 | TX underflow | Write `len=8` with one TX DWORD → `ERROR_OVERFLOW/UNDERFLOW`; `TRANSFER_ERR_STAT` latched + W1C clear. |
| 10 | IBI injection | `inject_ibi(0,0x42,{0xAA,0xBB})` → `IBI_PORT` status (`addr<<1|len<<8`) + payload DWORD. |
| 11 | Interrupt aggregation | Enable `RESP_READY` signal → `irq_o[0]` high while a response is pending, low after drain. |
| 12 | RESET_CONTROL | `CMD_QUEUE_RST` clears a queued command; register self-clears (reads 0). |
| 13 | Multi-instance isolation | Writes to instance 1 do not affect instance 0/2; per-instance DAT isolation. |
| 14 | transport_dbg | Back-door DAT write via socket + `dbg_read` snapshot match. |
| 15 | QUEUE_SIZE | Reflects configured CR (8) and IBI (8) depths. |
| 16 | CCI introspection | `access_delay_ns` mutate; `num_instances` reads the resolved value. |

---

## 3. Negative bench cases

| # | Area | Intent |
|---|------|--------|
| 1 | Constructor guards | `num_instances=0`, `=7`, `access_delay_ns<0`, `xfer_delay_ns<0`, `tx_fifo_depth=0` each raise `SC_REPORT_FATAL` (probed via throwing handler). |
| 2 | TLM errors | Non-word width, misaligned, streaming-width mismatch, byte-enable, out-of-aperture, bad command → the matching `TLM_*_ERROR_RESPONSE`. |
| 3 | RAZ/WI hole | Unmapped in-window offset reads 0, write accepted. |
| 4 | transport_dbg | Misaligned / bad-width / OOB / CSR (non-table) accesses return 0; DAT-window write/read round-trip returns 4. |

---

## 4. Pass / fail criteria

- Both benches print `ALL TESTS PASSED`; CTest reports `2/2` passing.
- `--asan` reports **0** errors.
- No SystemC `Error`/`Fatal` outside the negative bench's expected probes.

---

## 5. Known coverage gaps (by design)

The following are out of scope for the LT model and therefore unverified here
(see `01_..._Specification.md §12`): bit-level SCL/SDA waveforms, OD/PP timing
parameters, CDC/metastability, HDR modes, the TCRI recovery datapath, the
external 128-entry DAT/DCT memory interface, and scan/DFT.
