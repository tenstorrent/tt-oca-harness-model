# Mailbox IP - SystemC/TLM Test Plan

## Document Information
- **IP Name**: Mailbox
- **Document Version**: 2.0
- **Date**: 2026-03-17
- **Target Audience**: Verification Engineers, Model Developers
- **Purpose**: Comprehensive test coverage specification for Mailbox SystemC/TLM2.0 model

---

## Table of Contents
1. [Test Plan Overview](#test-plan-overview)
2. [Test Coverage Matrix](#test-coverage-matrix)
3. [Test Case Specifications](#test-case-specifications)

---

## Test Plan Overview

### Scope
This test plan covers `mailbox_ip` — one channel — which is the level these tests instantiate:
- All 10 registers with complete access validation
- Bidirectional FIFO data transfer operations
- All 3 interrupt types per port (WTIRQ, RTIRQ, EIRQ)
- Error detection and handling mechanisms, including error responses and the two sentinels
- Threshold saturation and retroactive triggering
- Register callbacks and side-effects
- Reset behavior
- Cross-port communication and status coherence

The `mailbox_unit_t<N>` wrapper above it — the eight-channel aperture and its address decode —
is deliberately **not** covered here. Its logic is a division and a bounds check, and what
actually needs proving about it is that the decode agrees with the platform address map and
that each channel's interrupt reaches the right PIC source. Both are integration properties,
so they are verified by `sep-mailbox-test` at platform level instead.

### Coverage Categories
1. **Basic Register Tests**: Reset values, RO/WO/RW access enforcement (testbench.cpp)
2. **Reset Tests**: FIFO and interrupt state clear on reset (FUNC-001)
3. **Register Callback Tests**: Write/read side-effects — IRQS W1C, IRQEN masking, WIRQT/RIRQT saturation (FUNC-002)
4. **FIFO Data Transfer Tests**: Bidirectional, simultaneous, min/max depth (FUNC-003)
5. **Status Monitoring Tests**: Empty, full, threshold flags (FUNC-004)
6. **Error Detection Tests**: Overflow, underflow, error flag accumulation, clear-on-read (FUNC-005)
7. **Threshold and Interrupt Tests**: WTIRQ/RTIRQ both ports, threshold saturation, retroactive trigger, boundary, configuration (FUNC-006)
8. **Control Operation Tests**: Write FIFO flush, read FIFO flush, dual-port coordination (FUNC-007)

### Exclusions
Per the exclusion criteria, the following are NOT tested:
- Pin-level electrical characteristics
- Clock/timing frequencies and physical timing
- Power/clock gating mechanisms
- Internal FIFO implementation details (pointer management, memory structure)
- Items listed in "Is Not Modeled" category (AxiAddrWidth, AxiDataWidth, test_i)

---

## Test Coverage Matrix

| Category | Test Suite | Test Count | Test IDs |
|----------|-----------|------------|----------|
| Basic Register Access | testbench.cpp | 5 | TC001–TC005 |
| Reset Behavior | FUNC-001 | 1 | TC006 |
| Register Callbacks | FUNC-002 | 4 | TC007–TC010 |
| FIFO Data Transfer | FUNC-003 | 6 | TC011–TC016 |
| Data Transfer Callbacks | FUNC-003 | 2 | TC017, TC018 |
| Cross-Port Communication | FUNC-003 | 3 | TC013, TC019, TC020 |
| FIFO Boundary | FUNC-003 | 2 | TC021, TC022 |
| Status Monitoring | FUNC-004 | 4 | TC023–TC026 |
| Error Detection | FUNC-005 | 4 | TC027, TC028, TC030, TC031 |
| Error Interrupts | FUNC-005 | 2 | TC029, TC032 |
| Threshold Interrupts | FUNC-006 | 4 | TC033–TC036 |
| Threshold Configuration | FUNC-006 | 5 | TC037–TC041 |
| Threshold Boundary | FUNC-006 | 2 | TC042, TC043 |
| Interrupt Configuration | FUNC-006 | 2 | TC044, TC045 |
| Control Operations | FUNC-007 | 3 | TC046–TC048 |
| **Total (unique)** | | **48** | |

> Note: TC016 (bidirectional simultaneous) spans both data transfer and cross-port categories; counted once.

---

## Test Case Specifications

### Tests 1–5: Basic Register Access (testbench.cpp)

| No. | Test Case Name | Description | Registers Accessed | Ports/Signals | Test Type |
|---------|---------------|-------------|-------------------|---------------|-----------|
| 1 | test_register_reset_values | Verify all registers reset to correct default values after rst_ni assertion. All 10 registers expected at 0x0. STATUS[0] (empty) reflects live FIFO state and will be 1 after reset. WO registers (WRITE_DATA, CTRL) are not readable; RO registers return 0x0. | All 10 registers | rst_ni, socket0, socket1 | Positive |
| 2 | test_read_only_register_protection | Verify RO registers (READ_DATA 0x08, STATUS 0x10, ERROR_FLAGS 0x18, IRQP 0x40) return 0 and ignore write operations. No error flags set on ignored writes. | READ_DATA, STATUS, ERROR_FLAGS, IRQP | socket0 | Negative |
| 3 | test_write_only_register_protection | Verify WO registers (WRITE_DATA 0x00, CTRL 0x48) return 0 on read. | WRITE_DATA, CTRL | socket0 | Negative |
| 4 | test_read_write_register_access | Verify RW registers (WIRQT 0x20, RIRQT 0x28, IRQS 0x30, IRQEN 0x38) support both read and write. Write known values, read back to confirm. Threshold registers return saturated shadow value. | WIRQT, RIRQT, IRQS, IRQEN | socket0 | Positive |
| 5 | test_asynchronous_reset_behavior | Write non-zero values to RW registers (WIRQT, RIRQT, IRQEN). Write data to FIFO. Assert rst_ni=0. Deassert rst_ni=1. Verify all RW registers return to 0x0, FIFO cleared, interrupts deasserted. | WIRQT, RIRQT, IRQEN, WRITE_DATA, STATUS | rst_ni, socket0, socket1, irq_o[0], irq_o[1] | Positive |

---

### Test 6: System Reset and Initialization (FUNC-001)

| No. | Test Case Name | Suite TC | Description | Registers Accessed | Ports/Signals | Test Type |
|---------|---------------|----------|-------------|-------------------|---------------|-----------|
| 6 | test_reset_fifo_interrupt_state | TC006 | Fill FIFOs with data. Set interrupt conditions (write to set IRQS, enable IRQEN). Assert rst_ni=0, deassert rst_ni=1. Verify: both FIFOs empty (STATUS[0]=1 for both ports), all IRQS=0x0, all IRQEN=0x0, ERROR_FLAGS=0x0, irq_o[0] and irq_o[1] at inactive level. | All registers | rst_ni, socket0, socket1, irq_o[0], irq_o[1] | Positive |

---

### Tests 7–10: Register Callback Verification (FUNC-002)

| No. | Test Case Name | Suite TC | Description | Registers Accessed | Ports/Signals | Test Type |
|---------|---------------|----------|-------------|-------------------|---------------|-----------|
| 7 | test_callback_irqs_write1clear | TC007 | Trigger WTIRQ by exceeding write threshold. Verify IRQS[0]=1. Write 0 to IRQS[0] — verify no effect (sticky). Write 1 to IRQS[0] — verify bit clears. Verify IRQP updates immediately. Verify irq_o deasserts. | IRQS, IRQP, IRQEN, WIRQT, WRITE_DATA | socket0, irq_o[0] | Positive |
| 8 | test_callback_irqen_masking | TC008 | Set IRQS[0]=1 via write threshold. With IRQEN[0]=0: verify IRQP[0]=0 and irq_o[0] inactive. Enable IRQEN[0]=1: verify IRQP[0]=1 and irq_o[0] asserts immediately (retroactive). Disable IRQEN[0]=0: verify IRQP[0]=0 and irq_o[0] deasserts. Re-enable: verify re-assertion. | IRQEN, IRQS, IRQP, WIRQT, WRITE_DATA | socket0, irq_o[0] | Positive |
| 9 | test_callback_wirqt_saturation_immediate | TC009 | Write WIRQT=0xFF — verify reads back as 7 (MailboxDepth−1). Write 5 entries, then write WIRQT=4: verify IRQS[0] and STATUS[2] set immediately (retroactive trigger on threshold write). | WIRQT, STATUS, IRQS, WRITE_DATA | socket0 | Positive |
| 10 | test_callback_rirqt_saturation_immediate | TC010 | Write RIRQT=0xFF on Port 1 — verify reads back as 7. Port 0 writes 5 entries. Write RIRQT=3 on Port 1: verify IRQS[1] and STATUS[3] set immediately on Port 1 (retroactive trigger). | RIRQT, STATUS, IRQS, WRITE_DATA | socket0, socket1 | Positive |

---

### Tests 11–22: Bidirectional FIFO Data Transfer (FUNC-003)

| No. | Test Case Name | Suite TC | Description | Registers Accessed | Ports/Signals | Test Type |
|---------|---------------|----------|-------------|-------------------|---------------|-----------|
| 11 | test_data_write_port0_read_port1 | TC011 | Write 64-bit data to Port 0 WRITE_DATA. Read from Port 1 READ_DATA. Verify data integrity and FIFO order (D1, D2, D3 written → D1, D2, D3 read). | WRITE_DATA, READ_DATA, STATUS | socket0, socket1 | Positive |
| 12 | test_data_write_port1_read_port0 | TC012 | Write 64-bit data to Port 1 WRITE_DATA. Read from Port 0 READ_DATA. Verify data integrity and FIFO ordering. | WRITE_DATA, READ_DATA, STATUS | socket0, socket1 | Positive |
| 13 | test_data_bidirectional_simultaneous | TC013 | Port 0 and Port 1 both write and read simultaneously. Verify independent FIFO paths: no contention, correct data on each side. | WRITE_DATA, READ_DATA, STATUS | socket0, socket1 | Positive |
| 14 | test_data_transfer_min_length | TC014 | Write 1 entry, read 1 entry. Verify FIFO transitions: empty→1 entry→empty. STATUS[0] (empty flag) tracks correctly. | WRITE_DATA, READ_DATA, STATUS | socket0, socket1 | Positive |
| 15 | test_data_transfer_typical_length | TC015 | Write multiple entries (≤ MailboxDepth/2). Read all entries. Verify data integrity, ordering, and STATUS flags throughout. | WRITE_DATA, READ_DATA, STATUS | socket0, socket1 | Positive |
| 16 | test_data_transfer_max_length | TC016 | Write MailboxDepth entries (fills FIFO). Verify STATUS[1]=1 (full). Peer reads all entries. Verify STATUS[0]=1 (empty). No data loss. | WRITE_DATA, READ_DATA, STATUS | socket0, socket1 | Positive |
| 17 | test_callback_write_data_enqueue | TC017 | Verify WRITE_DATA write callback: pre-check STATUS[1] before enqueue. If full: sets ERROR_FLAGS[1] and IRQS[2], returns error. If space: enqueues data, updates STATUS, triggers threshold comparison. Verify cross-port: data available on peer STATUS[0]. | WRITE_DATA, STATUS, ERROR_FLAGS, IRQS, WIRQT | socket0, socket1 | Positive |
| 18 | test_callback_read_data_dequeue | TC018 | Verify READ_DATA read callback: pre-check STATUS[0] before dequeue. If empty: sets ERROR_FLAGS[0] and IRQS[2], returns error. If data: dequeues oldest entry, updates STATUS, triggers threshold comparison. Verify cross-port STATUS[1] (full flag) updates. | READ_DATA, STATUS, ERROR_FLAGS, IRQS, RIRQT | socket0, socket1 | Positive |
| 19 | test_crossport_data_integrity | TC019 | Port 0 writes pattern {0xDEADBEEF12345678, 0xCAFEBABE87654321, 0x0123456789ABCDEF}. Port 1 reads and verifies exact pattern in order. Port 1 writes different pattern. Port 0 reads and verifies. Tests 64-bit data integrity. | WRITE_DATA, READ_DATA | socket0, socket1 | Positive |
| 20 | test_crossport_status_coherence | TC020 | Port 0 writes to fill FIFO. Verify Port 0 STATUS[1] (full) sets and Port 1 STATUS[0] (empty) clears. Port 1 reads to drain FIFO. Verify Port 1 STATUS[0] (empty) sets and Port 0 STATUS[1] (full) clears. Confirms bidirectional STATUS coherence. | WRITE_DATA, READ_DATA, STATUS | socket0, socket1 | Positive |
| 21 | test_boundary_fifo_depth_min | TC021 | Verify MailboxDepth=2 operation. Write 2 entries: STATUS[1]=1 (full). Peer reads 2: STATUS[0]=1 (empty). Verify threshold saturation: WIRQT=0xFF clamps to 1 (depth−1). | WRITE_DATA, READ_DATA, STATUS, WIRQT | socket0, socket1 | Positive |
| 22 | test_boundary_fifo_depth_max | TC022 | Verify maximum configured FIFO depth. Fill FIFO completely (MailboxDepth entries): STATUS[1]=1. Peer reads all: STATUS[0]=1. Verify no data loss at maximum capacity. | WRITE_DATA, READ_DATA, STATUS | socket0, socket1 | Positive |

---

### Tests 23–26: FIFO Status Monitoring (FUNC-004)

| No. | Test Case Name | Suite TC | Description | Registers Accessed | Ports/Signals | Test Type |
|---------|---------------|----------|-------------|-------------------|---------------|-----------|
| 23 | test_status_empty_flag | TC023 | Verify STATUS[0] (empty) flag. Initial: empty=1. Peer writes data: empty=0. Read all data: empty=1. Test both ports. | WRITE_DATA, READ_DATA, STATUS | socket0, socket1 | Positive |
| 24 | test_status_full_flag | TC024 | Verify STATUS[1] (full) flag. Write MailboxDepth entries: full=1. Peer reads one: full=0. Test both ports. | WRITE_DATA, READ_DATA, STATUS | socket0, socket1 | Positive |
| 25 | test_status_write_threshold_flag | TC025 | Verify STATUS[2] (write_level_above_thresh). Set WIRQT=N. Write N+1 entries: STATUS[2]=1. Peer reads until fill≤N: STATUS[2]=0. | WRITE_DATA, STATUS, WIRQT | socket0, socket1 | Positive |
| 26 | test_status_read_threshold_flag | TC026 | Verify STATUS[3] (read_level_above_thresh). Set RIRQT=N. Peer writes N+1 entries: STATUS[3]=1. Read until fill≤N: STATUS[3]=0. | READ_DATA, STATUS, RIRQT | socket0, socket1 | Positive |

---

### Tests 27–32: Error Detection and Reporting (FUNC-005)

| No. | Test Case Name | Suite TC | Description | Registers Accessed | Ports/Signals | Test Type |
|---------|---------------|----------|-------------|-------------------|---------------|-----------|
| 27 | test_error_write_to_full | TC027 | Fill write FIFO to capacity. Write one more entry: verify ERROR_FLAGS[1]=1 (write_error) and IRQS[2]=1 (eirq). Verify FIFO contents unchanged. | WRITE_DATA, STATUS, ERROR_FLAGS, IRQS | socket0, socket1 | Negative |
| 28 | test_error_read_from_empty | TC028 | Attempt read from empty FIFO. Verify ERROR_FLAGS[0]=1 (read_error) and IRQS[2]=1 (eirq). Verify no data returned. | READ_DATA, STATUS, ERROR_FLAGS, IRQS | socket0 | Negative |
| 29 | test_interrupt_eirq_port0 | TC029 | Enable IRQEN[2]=1 on Port 0. Trigger write-to-full error. Verify ERROR_FLAGS[1]=1, IRQS[2]=1, IRQP[2]=1, irq_o[0] asserts. Read ERROR_FLAGS to clear flags (does NOT clear IRQS[2]). W1C IRQS[2] to clear interrupt. Verify irq_o[0] deasserts. | ERROR_FLAGS, IRQEN, IRQS, IRQP, WRITE_DATA, STATUS | socket0, irq_o[0] | Negative |
| 30 | test_error_flag_accumulation | TC030 | Trigger both write-to-full and read-from-empty errors without reading ERROR_FLAGS between them. Verify both ERROR_FLAGS[0] and ERROR_FLAGS[1] are set simultaneously (bits accumulate via OR). Read ERROR_FLAGS: verify both bits in returned value. Read again: verify 0x0 (cleared). | WRITE_DATA, READ_DATA, STATUS, ERROR_FLAGS | socket0, socket1 | Negative |
| 31 | test_error_flag_clear_on_read | TC031 | Trigger error to set ERROR_FLAGS[1]. Read ERROR_FLAGS: verify bit[1]=1 returned. Read ERROR_FLAGS again immediately: verify 0x0 (clear-on-read). Verify IRQS[2] remains set (ERROR_FLAGS clear does NOT clear IRQS[2]). | ERROR_FLAGS, IRQS, WRITE_DATA, STATUS | socket0 | Positive |
| 32 | test_interrupt_eirq_port1 | TC032 | Enable IRQEN[2]=1 on Port 1. Trigger read-from-empty error on Port 1. Verify ERROR_FLAGS[0]=1, IRQS[2]=1, IRQP[2]=1, irq_o[1] asserts. Read ERROR_FLAGS to clear flags. W1C IRQS[2] to clear. Verify irq_o[1] deasserts. | ERROR_FLAGS, IRQEN, IRQS, IRQP, READ_DATA, STATUS | socket1, irq_o[1] | Negative |

---

### Tests 33–45: Threshold-Based Interrupt Generation (FUNC-006)

| No. | Test Case Name | Suite TC | Description | Registers Accessed | Ports/Signals | Test Type |
|---------|---------------|----------|-------------|-------------------|---------------|-----------|
| 33 | test_interrupt_wtirq_port0 | TC033 | Set WIRQT=3, IRQEN[0]=1 on Port 0. Write 4 entries (fill=4 > threshold=3). Verify IRQS[0]=1, IRQP[0]=1, STATUS[2]=1, irq_o[0] asserts. W1C IRQS[0]. Verify irq_o[0] deasserts. | WIRQT, IRQEN, IRQS, IRQP, WRITE_DATA, STATUS | socket0, irq_o[0] | Positive |
| 34 | test_interrupt_rtirq_port0 | TC034 | Set RIRQT=3, IRQEN[1]=1 on Port 0. Port 1 writes 4 entries to Port 0 read FIFO (fill=4 > threshold=3). Verify IRQS[1]=1, IRQP[1]=1, STATUS[3]=1, irq_o[0] asserts. W1C IRQS[1]. Verify irq_o[0] deasserts. | RIRQT, IRQEN, IRQS, IRQP, WRITE_DATA, STATUS | socket0, socket1, irq_o[0] | Positive |
| 35 | test_interrupt_wtirq_port1 | TC035 | Set WIRQT=3, IRQEN[0]=1 on Port 1. Write 4 entries on Port 1. Verify IRQS[0]=1, IRQP[0]=1, irq_o[1] asserts. W1C IRQS[0]. Verify irq_o[1] deasserts. | WIRQT, IRQEN, IRQS, IRQP, WRITE_DATA | socket1, irq_o[1] | Positive |
| 36 | test_interrupt_rtirq_port1 | TC036 | Set RIRQT=3, IRQEN[1]=1 on Port 1. Port 0 writes 4 entries to Port 1 read FIFO. Verify IRQS[1]=1, IRQP[1]=1, STATUS[3]=1, irq_o[1] asserts. W1C IRQS[1]. Verify irq_o[1] deasserts. | RIRQT, IRQEN, IRQS, IRQP, WRITE_DATA, STATUS | socket0, socket1, irq_o[1] | Positive |
| 37 | test_threshold_saturation_wirqt | TC037 | Write WIRQT=0xFF. Read back: verify saturated to MailboxDepth−1. Write 5 entries. Write WIRQT=4 (5>4): verify IRQS[0] and STATUS[2] set immediately (retroactive). | WIRQT, STATUS, IRQS, WRITE_DATA | socket0 | Positive |
| 38 | test_threshold_saturation_rirqt | TC038 | Write RIRQT=0xFF on Port 1. Read back: saturated to MailboxDepth−1. Port 0 writes 5 entries. Write RIRQT=3 on Port 1 (5>3): verify IRQS[1] and STATUS[3] set immediately. | RIRQT, STATUS, IRQS, WRITE_DATA | socket0, socket1 | Positive |
| 39 | test_threshold_zero_wirqt | TC039 | Set WIRQT=0. Write 1 entry (fill=1 > threshold=0): verify IRQS[0]=1 and STATUS[2]=1 triggered immediately. | WIRQT, STATUS, IRQS, IRQEN, WRITE_DATA | socket0, irq_o[0] | Positive |
| 40 | test_threshold_zero_rirqt | TC040 | Set RIRQT=0 on Port 0. Port 1 writes 1 entry (fill=1 > threshold=0): verify IRQS[1]=1 and STATUS[3]=1 on Port 0. | RIRQT, STATUS, IRQS, IRQEN, WRITE_DATA | socket0, socket1, irq_o[0] | Positive |
| 41 | test_threshold_retroactive_trigger | TC041 | Fill write FIFO to N entries. Write WIRQT threshold below N in same transaction. Verify IRQS[0] and STATUS[2] set immediately on write (retroactive trigger within the WIRQT write callback). Repeat for RIRQT. | WIRQT, RIRQT, STATUS, IRQS, WRITE_DATA | socket0, socket1 | Positive |
| 42 | test_boundary_threshold_max_value | TC042 | Set WIRQT=MailboxDepth−1. Write MailboxDepth entries (fill=depth > threshold=depth−1). Verify IRQS[0]=1 and STATUS[2]=1. Test RIRQT similarly. | WIRQT, RIRQT, STATUS, IRQS, WRITE_DATA | socket0, socket1 | Positive |
| 43 | test_boundary_threshold_equal_usage | TC043 | Set WIRQT=5. Write exactly 5 entries (fill=5, threshold=5). Verify IRQS[0]=0, STATUS[2]=0 (5 is NOT greater than 5 — strict greater-than). Write 6th entry (fill=6 > 5): verify IRQS[0]=1, STATUS[2]=1. | WIRQT, STATUS, IRQS, WRITE_DATA | socket0, socket1 | Positive |
| 44 | test_config_interrupt_level_triggered | TC044 | Configure IrqEdgeTrig=false, IrqActHigh=true. Trigger WTIRQ interrupt. Verify irq_o[0]=1 (asserted) and stays high while IRQP[0]=1. W1C IRQS[0]: verify irq_o[0]=0 (deasserts). | IRQS, IRQEN, IRQP, WIRQT, WRITE_DATA | socket0, irq_o[0] | Positive |
| 45 | test_config_interrupt_polarity | TC045 | Verify IrqActHigh=true: inactive=0, active=1. Trigger interrupt: irq_o=1. Clear: irq_o=0. Verify IrqActHigh=false: inactive=1, active=0. Trigger interrupt: irq_o=0. Clear: irq_o=1. | IRQS, IRQEN, IRQP, WIRQT, WRITE_DATA | socket0, irq_o[0] | Positive |

---

### Tests 46–48: Software-Controlled FIFO Management (FUNC-007)

| No. | Test Case Name | Suite TC | Description | Registers Accessed | Ports/Signals | Test Type |
|---------|---------------|----------|-------------|-------------------|---------------|-----------|
| 46 | test_ctrl_flush_write_fifo | TC046 | Fill Port 0 write FIFO with data. Write CTRL[0]=1 (wflush). Verify: STATUS[1]=0 (write FIFO not full), STATUS[2]=0 (threshold cleared), peer (Port 1) STATUS[0]=1 (read FIFO empty), STATUS[3]=0 (read threshold cleared). Verify data permanently discarded. Test Port 1 wflush symmetrically. | CTRL, STATUS, WRITE_DATA | socket0, socket1 | Positive |
| 47 | test_ctrl_flush_read_fifo | TC047 | Port 1 fills Port 0 read FIFO. Write CTRL[1]=1 (rflush) on Port 0. Verify: PORT 0 STATUS[0]=1 (read FIFO empty), STATUS[3]=0. Port 1 STATUS[1]=0 (write FIFO freed), STATUS[2]=0. Data permanently discarded. Test Port 1 rflush symmetrically. | CTRL, STATUS, WRITE_DATA | socket0, socket1 | Positive |
| 48 | test_ctrl_flush_dual_port_or | TC048 | Scenario A: Port 0 wflush clears Port 1 read FIFO. Scenario B: Port 1 wflush clears Port 0 read FIFO. Scenario C: Port 0 rflush clears own read FIFO + peer write FIFO. Scenario D: Port 1 rflush clears own read FIFO + peer write FIFO. Scenario E: Both ports flush simultaneously. | CTRL, STATUS, WRITE_DATA | socket0, socket1 | Positive |

---

## Test Execution Notes

### Port and Signal Names
| Port | Type | Description |
|------|------|-------------|
| `socket0` | `tlm_target_socket<32>` | Port 0 AXI4-Lite register interface |
| `socket1` | `tlm_target_socket<32>` | Port 1 AXI4-Lite register interface |
| `irq_o[0]` | `sc_out<bool>` | Port 0 interrupt output |
| `irq_o[1]` | `sc_out<bool>` | Port 1 interrupt output |
| `clk_i` | `sc_in<double>` | Abstract clock frequency (Hz) |
| `rst_ni` | `sc_in<bool>` | Active-low asynchronous reset |

### Register Names and Addresses
All register offsets are 8-byte aligned (fixed 64-bit register width):

| Register | Offset | Access | Reset |
|----------|--------|--------|-------|
| WRITE_DATA | 0x00 | WO | 0x0 |
| READ_DATA | 0x08 | RO | 0x0 |
| STATUS | 0x10 | RO | 0x0 |
| ERROR_FLAGS | 0x18 | RO | 0x0 |
| WIRQT | 0x20 | RW | 0x0 |
| RIRQT | 0x28 | RW | 0x0 |
| IRQS | 0x30 | RW | 0x0 |
| IRQEN | 0x38 | RW | 0x0 |
| IRQP | 0x40 | RO | 0x0 |
| CTRL | 0x48 | WO | 0x0 |

### Interrupt Mapping
| Bit | IRQS/IRQEN/IRQP | Source |
|-----|-----------------|--------|
| [0] | WTIRQ | Write FIFO fill > WIRQT threshold |
| [1] | RTIRQ | Read FIFO fill > RIRQT threshold |
| [2] | EIRQ | Write-to-full or read-from-empty error |

Each port generates an independent `irq_o[port]` = OR of all IRQP[port] bits.

### Register Access Behavior

Every refused access returns a TLM error response, mirroring the `SLVERR` the RTL raises on
AXI-Lite. Tests assert the response status, not just the data.

| Access Condition | Behavior |
|-----------------|----------|
| Access to an unmapped offset (outside `0x00..0x4F`) | data 0x0, error response |
| Write to RO register | Error response |
| Read from WO register (CTRL) | data 0x0, error response |
| Read from WRITE_DATA | data `0xFEEDC0DE`, **OKAY** — the one permitted WO read |
| Write to full FIFO (WRITE_DATA) | ERROR_FLAGS[1]=1, IRQS[2]=1, error response |
| Read from empty FIFO (READ_DATA) | data `0xFEEDDEAD`, ERROR_FLAGS[0]=1, IRQS[2]=1, error response |

### Special Behaviors
- **Clear-on-Read**: Reading ERROR_FLAGS returns current flags and atomically clears both bits; does **not** clear IRQS[2]
- **Write-1-to-Clear**: Writing 1 to an IRQS bit clears it; writing 0 has no effect
- **Self-Clearing**: CTRL register always reads as 0x0 (WO); flush executes synchronously on write
- **Hardware-Computed**: IRQP = IRQS & IRQEN (recomputed on every read from shadow state)
- **Threshold Saturation**: WIRQT/RIRQT values ≥ MailboxDepth saturate to (MailboxDepth − 1)
- **Threshold Readback**: Reads of WIRQT/RIRQT return the saturated shadow value
- **Retroactive Triggering**: Writing a lower threshold immediately re-evaluates FIFO fill level
- **Cross-Port**: Port 0 WRITE_DATA → `fifo_0_to_1` → Port 1 READ_DATA (and vice versa)
- **Flush Mechanism**: `sc_fifo` has no `clear()`; drain performed via `nb_read()` loop
- **Sentinels**: an empty-FIFO read returns `0xFEEDDEAD` and a `WRITE_DATA` read returns
  `0xFEEDC0DE`, so firmware can distinguish an empty mailbox from one holding zero. Covered
  by FUNC-005 and the basic register-access tests respectively

### Configuration Parameters (Test Defaults)
| Parameter | Default | Notes |
|-----------|---------|-------|
| `mailbox_depth` | 8 | Min=2; saturation limit = depth−1 = 7 |
| `irq_edge_trig` | false | Level-triggered |
| `irq_act_high` | true | Active-high; inactive = logic 0 |
| `memory_size` | 0x50 | TLM register space per port (80 bytes) |

---

## Document Metadata
- **Total Unique Test Cases**: 48
- **Basic Tests**: 5 (TC001–TC005)
- **FUNC-001 (Reset)**: 1
- **FUNC-002 (Callbacks)**: 4
- **FUNC-003 (Data Transfer)**: 12
- **FUNC-004 (Status)**: 4
- **FUNC-005 (Error)**: 6
- **FUNC-006 (Threshold/Interrupt)**: 13
- **FUNC-007 (Control)**: 3

---

**End of Test Plan**
