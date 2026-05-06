# AON Timer SystemC TLM Model - Test Plan

## Document Metadata

| Field | Value |
|---|---|
| IP Name | aon_timer |
| Document Type | Unit Test Plan |
| Model Type | SystemC TLM-2.0 Loosely Timed (LT) |
| Generation Date | 2026-02-19 |
| Source Documents | aon_timer-high-level-design.md, aon_timer-detailed-design.md, aon_timer-architecture-behaviour-map.json, aon_timer-features.md, aon_timer-memory-map-registers.md, aon_timer-register-callbacks.md, aon_timer-assumptions.md, aon_timer-port-interfaces.md |

---

## Table of Contents

1. [Overview](#1-overview)
2. [Test Environment Prerequisites](#2-test-environment-prerequisites)
3. [Register Map Reference](#3-register-map-reference)
4. [Port Interface Reference](#4-port-interface-reference)
5. [Test Cases](#5-test-cases)
   - 5.1 [Register Reset and Access Type Tests](#51-register-reset-and-access-type-tests)
   - 5.2 [Wakeup Timer Functional Tests](#52-wakeup-timer-functional-tests)
   - 5.3 [Watchdog Timer Functional Tests](#53-watchdog-timer-functional-tests)
   - 5.4 [Interrupt Tests](#54-interrupt-tests)
   - 5.5 [Power Management Tests](#55-power-management-tests)
   - 5.6 [Security Tests](#56-security-tests)
   - 5.7 [64-bit Non-Atomic Access Tests](#57-64-bit-non-atomic-access-tests)
   - 5.8 [Reset Behavior Tests](#58-reset-behavior-tests)
   - 5.9 [Escalation Tests](#59-escalation-tests)
   - 5.10 [Edge and Corner Case Tests](#510-edge-and-corner-case-tests)
6. [Coverage Summary](#6-coverage-summary)

---

## 1. Overview

The AON Timer (Always-On Timer) is a dual-timer peripheral comprising:
- A 64-bit upcounting wakeup timer (WKUP) with a 12-bit prescaler
- A 32-bit upcounting watchdog timer (WDOG) with dual bark (interrupt) and bite (reset) thresholds

The peripheral operates in the AON clock domain (~200 kHz) and provides interrupt outputs, power management wakeup requests, watchdog bite reset requests, and lifecycle escalation response.

This test plan validates the SystemC TLM-2.0 Loosely Timed model of this IP, covering all software-visible functional behavior. Tests verify register access semantics, timer counting and threshold logic, interrupt generation and clearing, power management outputs, security locking, non-atomic 64-bit access patterns, reset behavior, and escalation halting.

---

## 2. Test Environment Prerequisites

- SystemC TLM-2.0 simulation environment
- AON Timer model instantiated with default `EnableRacl=0` (unless testing RACL features)
- Clock frequency parameters: `clk_aon_freq` = 200000 Hz (200 kHz), `clk_sys_freq` = 100000000 Hz (100 MHz)
- Both resets (`rst_n`, `rst_aon_n`) asserted then de-asserted before each test
- `sleep_mode` = 0 (de-asserted) by default unless explicitly testing sleep pause
- `lc_escalate_en` = 0 (de-asserted) by default unless explicitly testing escalation
- All registers at their reset state at the start of each test case
- Test infrastructure capable of driving `tl_socket` TLM transactions, monitoring all output ports, and scheduling events in simulation time

---

## 3. Register Map Reference

| Register Name | Offset | Access Type | Reset Value | Key Fields |
|---|---|---|---|---|
| ALERT_TEST | 0x00 | WO | 0x00000000 | bit[0]: `fatal_fault` |
| WKUP_CTRL | 0x04 | RW | 0x00000000 | bit[0]: `enable`; bits[12:1]: `prescaler` |
| WKUP_THOLD_HI | 0x08 | RW | 0x00000000 | bits[31:0]: `threshold_hi` (upper 32 bits of 64-bit threshold) |
| WKUP_THOLD_LO | 0x0C | RW | 0x00000000 | bits[31:0]: `threshold_lo` (lower 32 bits of 64-bit threshold) |
| WKUP_COUNT_HI | 0x10 | RW | 0x00000000 | bits[31:0]: `count_hi` (upper 32 bits of 64-bit counter) |
| WKUP_COUNT_LO | 0x14 | RW | 0x00000000 | bits[31:0]: `count_lo` (lower 32 bits of 64-bit counter) |
| WDOG_REGWEN | 0x18 | RW0C | 0x00000001 | bit[0]: `regwen` (1=unlocked, 0=locked) |
| WDOG_CTRL | 0x1C | RW (gated) | 0x00000000 | bit[0]: `enable`; bit[1]: `pause_in_sleep` |
| WDOG_BARK_THOLD | 0x20 | RW (gated) | 0x00000000 | bits[31:0]: `threshold` |
| WDOG_BITE_THOLD | 0x24 | RW (gated) | 0x00000000 | bits[31:0]: `threshold` |
| WDOG_COUNT | 0x28 | RW | 0x00000000 | bits[31:0]: `count` |
| INTR_STATE | 0x2C | RW1C | 0x00000000 | bit[0]: `wkup_timer_expired`; bit[1]: `wdog_timer_bark` |
| INTR_TEST | 0x30 | WO | 0x00000000 | bit[0]: `wkup_timer_expired`; bit[1]: `wdog_timer_bark` |
| WKUP_CAUSE | 0x34 | RW0C | 0x00000000 | bit[0]: `cause` |

---

## 4. Port Interface Reference

| Port Name | Type | Direction | Description |
|---|---|---|---|
| `tl_socket` | tlm_target_socket<32> | Input | TL-UL register access interface |
| `intr_wkup_timer_expired` | sc_out<bool> | Output | Wakeup timer expiry interrupt (SYS domain, level-sensitive) |
| `intr_wdog_timer_bark` | sc_out<bool> | Output | Watchdog bark interrupt (SYS domain, level-sensitive) |
| `nmi_wdog_timer_bark` | sc_out<bool> | Output | NMI copy of watchdog bark interrupt (SYS domain) |
| `wkup_req` | sc_out<bool> | Output | Wakeup request to power manager (AON domain) |
| `aon_timer_rst_req` | sc_out<bool> | Output | Reset request to power manager (AON domain) |
| `sleep_mode` | sc_in<bool> | Input | Sleep mode indication from power manager |
| `lc_escalate_en` | sc_in<bool> | Input | Lifecycle escalation enable from lifecycle controller |
| `fatal_fault` | sc_out<bool> | Output | Fatal TL-UL bus integrity alert |
| `clk_aon_freq` | sc_in<double> | Input | AON clock frequency in Hz (abstract) |
| `clk_sys_freq` | sc_in<double> | Input | SYS clock frequency in Hz (abstract) |
| `rst_n` | sc_in<bool> | Input | Active-low SYS domain reset |
| `rst_aon_n` | sc_in<bool> | Input | Active-low AON domain reset |
| `racl_policies` | sc_in<racl_policy_vec_t> | Input | RACL policy vector (present only when EnableRacl=1) |
| `racl_error` | sc_out<racl_error_log_t> | Output | RACL violation log (present only when EnableRacl=1) |

---

## 5. Test Cases

---

### 5.1 Register Reset and Access Type Tests

---

#### TC_AON_001

**Test Name:** Register Reset Values Verification - All Registers

**Description:** Verify that all 14 AON Timer registers read back their documented reset values immediately after de-asserting the system reset. This test ensures the reset state of the entire peripheral is architecturally compliant.

**Prerequisites:**
- Model instantiated with `EnableRacl=0`
- `rst_n` and `rst_aon_n` asserted then de-asserted

**Test Steps:**
1. Assert `rst_n` = 0 and `rst_aon_n` = 0 to initiate reset
2. Wait for reset propagation (minimum 1 SYS clock cycle equivalent)
3. De-assert `rst_n` = 1 and `rst_aon_n` = 1
4. Issue a read transaction via `tl_socket` to offset 0x00 (ALERT_TEST) and record value
5. Issue a read transaction via `tl_socket` to offset 0x04 (WKUP_CTRL) and record value
6. Issue read transactions to offsets 0x08, 0x0C, 0x10, 0x14 (WKUP_THOLD_HI, WKUP_THOLD_LO, WKUP_COUNT_HI, WKUP_COUNT_LO) and record values
7. Issue a read transaction to offset 0x18 (WDOG_REGWEN) and record value
8. Issue read transactions to offsets 0x1C, 0x20, 0x24, 0x28 (WDOG_CTRL, WDOG_BARK_THOLD, WDOG_BITE_THOLD, WDOG_COUNT) and record values
9. Issue read transactions to offsets 0x2C, 0x30, 0x34 (INTR_STATE, INTR_TEST, WKUP_CAUSE) and record values

**Expected Results:**
- ALERT_TEST (0x00) reads 0x00000000
- WKUP_CTRL (0x04) reads 0x00000000
- WKUP_THOLD_HI (0x08) reads 0x00000000
- WKUP_THOLD_LO (0x0C) reads 0x00000000
- WKUP_COUNT_HI (0x10) reads 0x00000000
- WKUP_COUNT_LO (0x14) reads 0x00000000
- WDOG_REGWEN (0x18) reads 0x00000001 (unlocked)
- WDOG_CTRL (0x1C) reads 0x00000000
- WDOG_BARK_THOLD (0x20) reads 0x00000000
- WDOG_BITE_THOLD (0x24) reads 0x00000000
- WDOG_COUNT (0x28) reads 0x00000000
- INTR_STATE (0x2C) reads 0x00000000
- INTR_TEST (0x30) reads 0x00000000
- WKUP_CAUSE (0x34) reads 0x00000000
- Output ports `intr_wkup_timer_expired`, `intr_wdog_timer_bark`, `nmi_wdog_timer_bark`, `wkup_req`, `aon_timer_rst_req` all read as logic 0

**Pass/Fail Criteria:** All register read values match their documented reset values exactly. All output ports are deasserted.

---

#### TC_AON_002

**Test Name:** WKUP_CTRL RW Access and Reserved Bits Behavior

**Description:** Verify that WKUP_CTRL allows read-write access to its defined fields (bits 12:1 prescaler, bit 0 enable), that reserved bits (31:13) read as zero, and that writes to reserved bits are silently ignored without corrupting the valid fields.

**Prerequisites:**
- Reset applied, all registers at reset state

**Test Steps:**
1. Write 0x00001FFF to WKUP_CTRL via `tl_socket` (all defined bits set: prescaler=0xFFF, enable=1)
2. Perform read-back of WKUP_CTRL and record value
3. Write 0xFFFFFFFF to WKUP_CTRL (attempt to set all bits including reserved)
4. Perform read-back of WKUP_CTRL and record value
5. Write 0x00000000 to WKUP_CTRL (clear all fields)
6. Perform read-back and verify

**Expected Results:**
- After writing 0x00001FFF: read returns 0x00001FFF (prescaler=0xFFF, enable=1)
- After writing 0xFFFFFFFF: read returns 0x00001FFF (reserved bits 31:13 read as zero; valid fields retain written values)
- After writing 0x00000000: read returns 0x00000000

**Pass/Fail Criteria:** Reserved bits always read as zero. Only bits 12:0 are writable and readable. No error response from `tl_socket` on any access.

---

#### TC_AON_003

**Test Name:** ALERT_TEST Write-Only (WO) Access - No Read Storage

**Description:** Verify that ALERT_TEST is a write-only register with no storage. Reads must return 0x00000000 regardless of what was written, and writes to bit[0] must trigger a `fatal_fault` output pulse.

**Prerequisites:**
- Reset applied

**Test Steps:**
1. Read ALERT_TEST via `tl_socket` before any write; record value
2. Write 0x00000001 to ALERT_TEST via `tl_socket`
3. Immediately read ALERT_TEST and record value
4. Monitor `fatal_fault` output during and after the write in step 2
5. Write 0xFFFFFFFF to ALERT_TEST
6. Read ALERT_TEST and record value

**Expected Results:**
- All reads of ALERT_TEST return 0x00000000 regardless of prior writes (no storage)
- A transient pulse on `fatal_fault` output is observed when bit[0] is written as 1
- `fatal_fault` does not latch; it returns to 0 after the test pulse
- Reserved bits[31:1] writes produce no side effect

**Pass/Fail Criteria:** Read of ALERT_TEST always returns 0x00000000. `fatal_fault` asserts transiently on bit[0] write-1.

---

#### TC_AON_004

**Test Name:** INTR_TEST Write-Only (WO) Access - No Read Storage

**Description:** Verify that INTR_TEST is a write-only register with no storage. Reads must always return 0x00000000. Writes to defined bits force-assert corresponding interrupt outputs.

**Prerequisites:**
- Reset applied, timers disabled

**Test Steps:**
1. Read INTR_TEST via `tl_socket` before any write; record value
2. Write 0x00000001 to INTR_TEST (bit[0] = wkup_timer_expired)
3. Read INTR_TEST immediately; record value
4. Write 0x00000002 to INTR_TEST (bit[1] = wdog_timer_bark)
5. Read INTR_TEST immediately; record value
6. Write 0x00000003 to INTR_TEST (both bits)
7. Read INTR_TEST; record value

**Expected Results:**
- All reads of INTR_TEST return 0x00000000 (no storage flip-flops)
- Reserved bits[31:2] writes are ignored; reads return 0

**Pass/Fail Criteria:** Every read of INTR_TEST returns 0x00000000. No error responses from `tl_socket`.

---

#### TC_AON_005

**Test Name:** INTR_STATE RW1C Access Semantics

**Description:** Verify that INTR_STATE implements correct W1C (write-1-to-clear) semantics. Writing 1 to a set bit clears it and de-asserts the interrupt output. Writing 0 to a set bit has no effect.

**Prerequisites:**
- Reset applied
- Use INTR_TEST to force-assert both interrupt bits

**Test Steps:**
1. Write 0x00000003 to INTR_TEST to force-set both INTR_STATE bits
2. Read INTR_STATE; verify both bits are set (0x00000003)
3. Write 0x00000000 to INTR_STATE (attempt to clear with all-zero write)
4. Read INTR_STATE; verify bits are still set (W0 has no effect)
5. Write 0x00000001 to INTR_STATE (write 1 to bit[0] only)
6. Read INTR_STATE; verify bit[0] is cleared, bit[1] remains set (0x00000002)
7. Monitor `intr_wkup_timer_expired` output; it must deassert after step 5
8. Monitor `intr_wdog_timer_bark` output; it must remain asserted after step 5
9. Write 0x00000002 to INTR_STATE (write 1 to bit[1])
10. Read INTR_STATE; verify 0x00000000
11. Monitor `intr_wdog_timer_bark` and `nmi_wdog_timer_bark`; both must deassert

**Expected Results:**
- Writing 0 to INTR_STATE does not clear any bits
- Writing 1 to a bit clears that bit and de-asserts the corresponding output signal
- Clearing bit[1] de-asserts both `intr_wdog_timer_bark` AND `nmi_wdog_timer_bark` simultaneously

**Pass/Fail Criteria:** Only write-1 operations clear INTR_STATE bits. Write-0 has no effect. NMI mirrors bark interrupt at all times.

---

#### TC_AON_006

**Test Name:** WKUP_CAUSE RW0C Access Semantics

**Description:** Verify that WKUP_CAUSE implements correct RW0C (read-write-0-to-clear) semantics. Writing 0 to bit[0] clears it and de-asserts `wkup_req`. Writing 1 has no effect.

**Prerequisites:**
- Reset applied
- Wakeup timer configured to trigger threshold crossing to set WKUP_CAUSE

**Test Steps:**
1. Configure WKUP_CTRL: write 0x00000001 (enable=1, prescaler=0) via `tl_socket`
2. Write WKUP_THOLD_HI = 0x00000000, WKUP_THOLD_LO = 0x00000002 (threshold=2)
3. Write WKUP_COUNT_HI = 0x00000000, WKUP_COUNT_LO = 0x00000000 (counter=0)
4. Advance simulation time until threshold is crossed
5. Read WKUP_CAUSE; verify bit[0]=1 and `wkup_req` output is asserted
6. Write 0x00000001 to WKUP_CAUSE (attempt RW0C clear with value 1)
7. Read WKUP_CAUSE; verify bit[0] remains 1 and `wkup_req` remains asserted
8. Write 0x00000000 to WKUP_CAUSE (correct RW0C clear)
9. Read WKUP_CAUSE; verify bit[0]=0 and `wkup_req` is de-asserted

**Expected Results:**
- Writing 1 to WKUP_CAUSE.cause has no effect
- Writing 0 to WKUP_CAUSE.cause clears the bit and de-asserts `wkup_req`

**Pass/Fail Criteria:** WKUP_CAUSE correctly implements RW0C semantics. `wkup_req` tracks WKUP_CAUSE.cause faithfully.

---

#### TC_AON_007

**Test Name:** WDOG_REGWEN RW0C Write-Once-Clear Lock Semantics

**Description:** Verify that WDOG_REGWEN.regwen (bit[0]) starts at reset value 1, can be cleared to 0 by writing 0, and cannot be restored to 1 without a system reset. Verify writing 1 has no effect.

**Prerequisites:**
- Reset applied

**Test Steps:**
1. Read WDOG_REGWEN via `tl_socket`; verify value = 0x00000001
2. Write 0x00000001 to WDOG_REGWEN (attempt to write 1 to an already-set bit)
3. Read WDOG_REGWEN; verify still 0x00000001 (write-1 has no effect)
4. Write 0x00000000 to WDOG_REGWEN (lock the watchdog)
5. Read WDOG_REGWEN; verify value = 0x00000000 (locked)
6. Write 0x00000001 to WDOG_REGWEN (attempt to restore - must have no effect)
7. Read WDOG_REGWEN; verify value still 0x00000000
8. Assert and de-assert `rst_n` (system reset)
9. Read WDOG_REGWEN; verify value restored to 0x00000001

**Expected Results:**
- Reset value of WDOG_REGWEN = 0x00000001
- Writing 0 clears bit[0] permanently within the current power cycle
- Writing 1 has no effect regardless of current state
- System reset (`rst_n`) restores WDOG_REGWEN to 0x00000001

**Pass/Fail Criteria:** Lock is irreversible without system reset. Reset correctly restores the unlock state.

---

#### TC_AON_008

**Test Name:** Reserved Bits Read-As-Zero Across All Registers

**Description:** Verify that reserved bit fields in all registers with partial bit usage return zero on read, and that writes to reserved bits are silently ignored without corrupting defined fields.

**Prerequisites:**
- Reset applied

**Test Steps:**
1. Write 0xFFFFFFFF to WKUP_CTRL; read back and check bits[31:13] = 0, bits[12:0] = 0x1FFF
2. Write 0xFFFFFFFF to WDOG_REGWEN; read back and check bits[31:1] = 0, bit[0] = as per current lock state
3. Write 0xFFFFFFFF to WDOG_CTRL; read back and check bits[31:2] = 0, bits[1:0] = 0x3
4. Write 0xFFFFFFFF to INTR_STATE; read back after W1C settles; check bits[31:2] = 0
5. Write 0xFFFFFFFF to WKUP_CAUSE; read back and check bits[31:1] = 0
6. Write 0xFFFFFFFF to INTR_TEST; read back and check value = 0x00000000 (write-only)

**Expected Results:**
- All reserved bits read as zero regardless of the value written
- Defined fields in each register retain their written values (subject to W1C/RW0C semantics)
- No error response from `tl_socket` on any access

**Pass/Fail Criteria:** Reserved bits return 0 on all reads. No register corruption occurs.

---

#### TC_AON_009

**Test Name:** Asynchronous Register Write Completion - Read-Back Guarantees Write Propagation

**Description:** Verify the CDC write-completion semantics. Writes to AON Timer registers complete at the TL-UL interface before taking effect in the AON domain. A subsequent read-back of the same register must stall until the write has propagated and return the written value.

**Prerequisites:**
- Reset applied, timers disabled

**Test Steps:**
1. Write 0x00000001 to WKUP_CTRL via `tl_socket` (enable wakeup timer, prescaler=0)
2. Immediately issue a read transaction to WKUP_CTRL without any simulation time advancement
3. Record the value returned and the simulation time at which the response arrives
4. Verify that the read response includes the written value (0x00000001)
5. Repeat steps 1-4 for WDOG_CTRL (write 0x00000001)
6. Repeat for WDOG_BARK_THOLD (write 0x00001000) and read back
7. Repeat for WKUP_THOLD_HI (write 0xDEADBEEF) and read back

**Expected Results:**
- Each read-back returns the previously written value
- The read-back completes after the write has propagated through the functional CDC model
- No stale or pre-write values are returned by read-back operations

**Pass/Fail Criteria:** Read-back after write always returns the written value, confirming write propagation. The model correctly stalls read completion until CDC propagation is functionally complete.

---

### 5.2 Wakeup Timer Functional Tests

---

#### TC_AON_010

**Test Name:** Wakeup Timer Enable and Disable via WKUP_CTRL.enable

**Description:** Verify that the wakeup timer only counts when WKUP_CTRL.enable is 1, stops counting when cleared to 0, and resumes counting from the halted counter value when re-enabled.

**Prerequisites:**
- Reset applied, `lc_escalate_en` = 0

**Test Steps:**
1. Write WKUP_THOLD_LO = 0xFFFFFFFF, WKUP_THOLD_HI = 0xFFFFFFFF (set maximum threshold to prevent threshold crossing)
2. Write WKUP_COUNT_HI = 0x00000000, WKUP_COUNT_LO = 0x00000000 (initialize counter to 0)
3. Write WKUP_CTRL = 0x00000000 (timer disabled, prescaler=0); perform read-back
4. Advance simulation by 10 AON ticks; read WKUP_COUNT_LO; record value A (must be 0 - counter should not advance while disabled)
5. Write WKUP_CTRL = 0x00000001 (enable=1, prescaler=0); perform read-back
6. Advance simulation by 5 AON ticks; read WKUP_COUNT_LO; record value B (must be approximately 5)
7. Write WKUP_CTRL = 0x00000000 (disable); perform read-back
8. Record value C by reading WKUP_COUNT_LO immediately after disabling
9. Advance simulation by 10 AON ticks; read WKUP_COUNT_LO; record value D (must equal C - counter should not advance while disabled)
10. Write WKUP_CTRL = 0x00000001 (re-enable); advance 5 ticks; read WKUP_COUNT_LO; record value E (must be approximately C+5)

**Expected Results:**
- Value A = 0 (disabled timer does not count)
- Value B >= 5 (enabled timer counts forward at prescaler=0 rate)
- Value D = Value C (disabled timer holds its count)
- Value E >= C + 5 (resumed timer continues from where it stopped)

**Pass/Fail Criteria:** Counter only advances when WKUP_CTRL.enable=1 and escalation is not asserted.

---

#### TC_AON_011

**Test Name:** Wakeup Timer Prescaler Operation - Various Prescaler Values

**Description:** Verify that the wakeup timer prescaler correctly divides the AON clock rate. With prescaler N, the counter should increment once every N+1 AON ticks. Test prescaler=0 (maximum rate), prescaler=1 (half rate), and prescaler=4095 (minimum rate).

**Prerequisites:**
- Reset applied, WKUP_THOLD set to maximum to prevent threshold crossing

**Test Steps:**
1. Set WKUP_THOLD_HI = 0xFFFFFFFF, WKUP_THOLD_LO = 0xFFFFFFFF
2. Write WKUP_COUNT to 0 (both HI and LO)
3. Write WKUP_CTRL = 0x00000001 (prescaler=0, enable=1); perform read-back
4. Advance simulation by exactly 10 AON ticks; read WKUP_COUNT_LO; record value P0 (expected ~10)
5. Disable timer; reset WKUP_COUNT to 0
6. Write WKUP_CTRL = 0x00000003 (prescaler=1, enable=1); perform read-back
7. Advance simulation by exactly 10 AON ticks; read WKUP_COUNT_LO; record value P1 (expected ~5)
8. Disable timer; reset WKUP_COUNT to 0
9. Write WKUP_CTRL = 0x00001FFF (prescaler=0xFFF=4095, enable=1); perform read-back
10. Advance simulation by exactly 4096 AON ticks; read WKUP_COUNT_LO; record value P4095 (expected ~1)

**Expected Results:**
- P0 is approximately 10 (one count per AON tick)
- P1 is approximately 5 (one count per 2 AON ticks)
- P4095 is approximately 1 (one count per 4096 AON ticks)

**Pass/Fail Criteria:** Counter increment rate matches prescaler formula: rate = `clk_aon_freq / (prescaler + 1)`.

---

#### TC_AON_012

**Test Name:** Prescaler Reset Side-Effect on Every WKUP_CTRL Write

**Description:** Verify that every write to WKUP_CTRL, including writes that do not change the value, unconditionally resets the internal prescaler accumulator to zero. This side-effect is observable through the counting rate.

**Prerequisites:**
- Reset applied, WKUP_THOLD set to maximum

**Test Steps:**
1. Write WKUP_CTRL = 0x00000005 (prescaler=2, enable=1); perform read-back
2. Advance simulation by 2 AON ticks (partially through a prescaler period with prescaler=2)
3. Record WKUP_COUNT_LO value (expected = 0, as 3 ticks are needed for first count)
4. Write WKUP_CTRL = 0x00000005 (same value - no field change); perform read-back
5. Advance simulation by 2 more AON ticks
6. Record WKUP_COUNT_LO (if prescaler were not reset, the 2 earlier ticks would carry over and 4 total ticks would yield 1 count; with prescaler reset, 2 ticks still yields 0)
7. Advance 1 more AON tick (total 3 since last write)
8. Record WKUP_COUNT_LO; expected = 1 (3 ticks with prescaler=2 means first count at tick 3)

**Expected Results:**
- After step 4, the prescaler accumulator restarts from 0 despite no value change
- The counting delay restarts from the full prescaler period after each WKUP_CTRL write
- WKUP_COUNT advances only after prescaler+1 ticks from the last WKUP_CTRL write

**Pass/Fail Criteria:** Prescaler accumulator resets on every WKUP_CTRL write, even if no field changes.

---

#### TC_AON_013

**Test Name:** Wakeup Timer Threshold Comparison and Interrupt Generation

**Description:** Verify that the wakeup timer asserts `intr_wkup_timer_expired`, sets INTR_STATE.wkup_timer_expired, sets WKUP_CAUSE.cause, and asserts `wkup_req` when WKUP_COUNT reaches or exceeds WKUP_THOLD. Verify the threshold comparison uses >= semantics (exact match and overshoot both trigger).

**Prerequisites:**
- Reset applied, `sleep_mode`=0, `lc_escalate_en`=0

**Test Steps:**
1. Write WKUP_THOLD_LO = 0x00000005, WKUP_THOLD_HI = 0x00000000 (threshold = 5)
2. Write WKUP_COUNT_HI = 0x00000000, WKUP_COUNT_LO = 0x00000000
3. Write WKUP_CTRL = 0x00000001 (prescaler=0, enable=1); perform read-back
4. Monitor `intr_wkup_timer_expired` output; advance simulation one tick at a time
5. At each tick, record WKUP_COUNT_LO and `intr_wkup_timer_expired` state
6. Verify `intr_wkup_timer_expired` is 0 for counts 0 through 4
7. Verify `intr_wkup_timer_expired` asserts at count = 5 (exact threshold)
8. Read INTR_STATE; verify bit[0] = 1
9. Read WKUP_CAUSE; verify bit[0] = 1 (cause set)
10. Verify `wkup_req` output is asserted

**Expected Results:**
- `intr_wkup_timer_expired` = 0 when WKUP_COUNT < WKUP_THOLD
- `intr_wkup_timer_expired` = 1 when WKUP_COUNT >= WKUP_THOLD
- INTR_STATE.wkup_timer_expired = 1 upon threshold crossing
- WKUP_CAUSE.cause = 1 and `wkup_req` = 1 upon threshold crossing
- Counter continues incrementing beyond threshold (no auto-disable)

**Pass/Fail Criteria:** All four outputs assert simultaneously at the threshold crossing. Comparison uses >= semantics. Counter does not stop at threshold.

---

#### TC_AON_014

**Test Name:** Wakeup Timer Interrupt Continuous Re-Triggering After INTR_STATE Clear

**Description:** Verify that clearing INTR_STATE.wkup_timer_expired via W1C while the counter remains above the threshold causes the interrupt to immediately re-assert on the next AON clock tick, producing continuous interrupts until the threshold condition is resolved.

**Prerequisites:**
- Reset applied, wakeup timer running with count above threshold from TC_AON_013 or similar setup

**Test Steps:**
1. Configure and enable wakeup timer with threshold=3; allow count to reach 5
2. Verify `intr_wkup_timer_expired` is asserted, INTR_STATE[0]=1
3. Write 0x00000001 to INTR_STATE (W1C clear); read INTR_STATE
4. Verify INTR_STATE[0] = 0 immediately after write
5. Advance simulation by 1 AON tick
6. Read INTR_STATE[0] and monitor `intr_wkup_timer_expired`
7. Verify interrupt re-asserts (INTR_STATE[0]=1 again) because count is still >= threshold
8. Disable timer by writing WKUP_CTRL = 0x00000000; clear INTR_STATE again
9. Advance 5 ticks; verify interrupt does not re-assert (timer is disabled)

**Expected Results:**
- After W1C clear, interrupt de-asserts momentarily then re-asserts on next AON tick while count >= threshold and timer enabled
- After disabling the timer, interrupt does not re-assert after W1C clear

**Pass/Fail Criteria:** Interrupt re-assertion after clear is correctly modeled when threshold condition persists. Disabling the timer prevents re-assertion.

---

#### TC_AON_015

**Test Name:** Wakeup Timer 64-bit Threshold - Full 64-bit Comparison

**Description:** Verify that the threshold comparison uses the full 64-bit value assembled from both WKUP_THOLD_HI and WKUP_THOLD_LO, and that the interrupt fires correctly when the concatenated 64-bit counter equals or exceeds the 64-bit threshold.

**Prerequisites:**
- Reset applied

**Test Steps:**
1. Write WKUP_THOLD_LO = 0x00000000, WKUP_THOLD_HI = 0x00000001 (64-bit threshold = 0x00000001_00000000)
2. Write WKUP_COUNT_HI = 0x00000000, WKUP_COUNT_LO = 0xFFFFFFFE (counter near LO overflow)
3. Write WKUP_CTRL = 0x00000001 (prescaler=0, enable=1); perform read-back
4. Advance simulation by 1 tick; verify count = 0x00000000_FFFFFFFF; verify `intr_wkup_timer_expired` = 0
5. Advance simulation by 1 more tick; verify count = 0x00000001_00000000; verify `intr_wkup_timer_expired` asserts

**Expected Results:**
- No interrupt at count 0x00000000_FFFFFFFF (below 0x00000001_00000000)
- Interrupt asserts exactly at count = 0x00000001_00000000 (matches 64-bit threshold)
- The LO overflow into HI is correctly handled in the threshold comparison

**Pass/Fail Criteria:** 64-bit threshold comparison is correct including carry from LO to HI.

---

#### TC_AON_016

**Test Name:** Wakeup Timer Counter Software Write - Initialize to Arbitrary Value

**Description:** Verify that software can write WKUP_COUNT_HI and WKUP_COUNT_LO to initialize the counter to an arbitrary value, and that the timer counts forward from the written value.

**Prerequisites:**
- Reset applied, timer disabled before counter write

**Test Steps:**
1. Write WKUP_CTRL = 0x00000000 (disable timer)
2. Write WKUP_THOLD_HI = 0x00000002, WKUP_THOLD_LO = 0x00000005 (threshold = 0x00000002_00000005)
3. Write WKUP_COUNT_HI = 0x00000002, WKUP_COUNT_LO = 0x00000000 (start at 0x00000002_00000000)
4. Write WKUP_CTRL = 0x00000001 (re-enable); perform read-back
5. Advance simulation; verify counter advances from 0x00000002_00000000
6. Verify `intr_wkup_timer_expired` asserts when count reaches 0x00000002_00000005
7. Read WKUP_COUNT_HI and WKUP_COUNT_LO; verify count is at or beyond threshold

**Expected Results:**
- After writing counter registers and re-enabling, counter advances from the written value
- Threshold crossing occurs at the correct 64-bit count value
- Interrupt and wakeup signals assert at the expected time

**Pass/Fail Criteria:** Counter initializes to the software-written value and counts forward from that point.

---

### 5.3 Watchdog Timer Functional Tests

---

#### TC_AON_017

**Test Name:** Watchdog Timer Enable and Disable via WDOG_CTRL.enable

**Description:** Verify that the watchdog timer counts only when WDOG_CTRL.enable is 1, and halts when cleared to 0.

**Prerequisites:**
- Reset applied, `lc_escalate_en`=0, `sleep_mode`=0

**Test Steps:**
1. Write WDOG_BARK_THOLD = 0xFFFFFFFF, WDOG_BITE_THOLD = 0xFFFFFFFF (maximum thresholds)
2. Write WDOG_COUNT = 0x00000000 (initialize to 0)
3. Write WDOG_CTRL = 0x00000000 (disabled); perform read-back
4. Advance simulation by 10 AON ticks; read WDOG_COUNT; verify = 0 (no counting while disabled)
5. Write WDOG_CTRL = 0x00000001 (enable=1); perform read-back
6. Advance simulation by 8 AON ticks; read WDOG_COUNT; record value (expected ~8)
7. Write WDOG_CTRL = 0x00000000 (disable); read WDOG_COUNT; record frozen value
8. Advance simulation by 10 more AON ticks; read WDOG_COUNT; verify no change (counter halted)

**Expected Results:**
- WDOG_COUNT does not advance when WDOG_CTRL.enable=0
- WDOG_COUNT advances at AON clock rate when enable=1
- Counter halts at the value it had when disabled

**Pass/Fail Criteria:** Watchdog counter follows enable/disable precisely with no spurious counts.

---

#### TC_AON_018

**Test Name:** Watchdog Bark Threshold Interrupt Generation

**Description:** Verify that the watchdog bark interrupt asserts `intr_wdog_timer_bark`, `nmi_wdog_timer_bark`, sets INTR_STATE.wdog_timer_bark, and asserts `wkup_req` when WDOG_COUNT reaches or exceeds WDOG_BARK_THOLD.

**Prerequisites:**
- Reset applied, `sleep_mode`=0, `lc_escalate_en`=0

**Test Steps:**
1. Write WDOG_BITE_THOLD = 0xFFFFFFFF (set bite threshold to maximum to prevent reset)
2. Write WDOG_BARK_THOLD = 0x00000005
3. Write WDOG_COUNT = 0x00000000
4. Write WDOG_CTRL = 0x00000001 (enable=1); perform read-back
5. Advance simulation one tick at a time; at each tick record WDOG_COUNT and `intr_wdog_timer_bark`
6. Verify `intr_wdog_timer_bark` = 0 while WDOG_COUNT < 5
7. Verify `intr_wdog_timer_bark` asserts when WDOG_COUNT = 5
8. Verify `nmi_wdog_timer_bark` asserts simultaneously with `intr_wdog_timer_bark`
9. Read INTR_STATE; verify bit[1]=1 (wdog_timer_bark set)
10. Read WKUP_CAUSE; verify bit[0]=1 (wakeup event from bark)
11. Verify `wkup_req` output is asserted

**Expected Results:**
- Bark interrupt asserts at count = WDOG_BARK_THOLD (>= comparison)
- `nmi_wdog_timer_bark` is identical to `intr_wdog_timer_bark`
- INTR_STATE.wdog_timer_bark=1, WKUP_CAUSE.cause=1, `wkup_req`=1 all assert simultaneously

**Pass/Fail Criteria:** All four bark-related outputs assert at the same time at the correct count value.

---

#### TC_AON_019

**Test Name:** Watchdog Bite Threshold Reset Request Generation

**Description:** Verify that the watchdog bite threshold asserts `aon_timer_rst_req` when WDOG_COUNT reaches or exceeds WDOG_BITE_THOLD, and that this is independent of the bark interrupt path.

**Prerequisites:**
- Reset applied, `sleep_mode`=0, `lc_escalate_en`=0

**Test Steps:**
1. Write WDOG_BARK_THOLD = 0x00000003 (bark at count 3)
2. Write WDOG_BITE_THOLD = 0x00000007 (bite at count 7)
3. Write WDOG_COUNT = 0x00000000
4. Write WDOG_CTRL = 0x00000001 (enable=1); perform read-back
5. Advance simulation to count=3; verify bark outputs assert (`intr_wdog_timer_bark`=1, `nmi_wdog_timer_bark`=1, INTR_STATE[1]=1)
6. Verify `aon_timer_rst_req` = 0 at count=3 (bite not yet triggered)
7. Continue advancing simulation; verify `aon_timer_rst_req` remains 0 while count < 7
8. Advance to count=7; verify `aon_timer_rst_req` = 1
9. Verify bark interrupts remain asserted alongside bite reset
10. Verify `intr_wkup_timer_expired` = 0 (bite does not affect wakeup interrupt)

**Expected Results:**
- `aon_timer_rst_req` asserts only when WDOG_COUNT >= WDOG_BITE_THOLD
- Bark and bite are independent paths; both can be simultaneously active
- Bite does not affect `intr_wkup_timer_expired`

**Pass/Fail Criteria:** `aon_timer_rst_req` asserts at the correct bite threshold count. Bark and bite paths operate independently.

---

#### TC_AON_020

**Test Name:** Watchdog Petting - Any Write to WDOG_COUNT Resets Counter to Zero

**Description:** Verify that any write to WDOG_COUNT resets the watchdog counter to zero, regardless of the written data value. Verify that both 0x00000000 and non-zero values produce identical petting behavior.

**Prerequisites:**
- Reset applied, watchdog enabled, bark threshold set sufficiently high

**Test Steps:**
1. Write WDOG_BARK_THOLD = 0x000000FF, WDOG_BITE_THOLD = 0xFFFFFFFF
2. Write WDOG_CTRL = 0x00000001; perform read-back
3. Advance simulation by 50 AON ticks; read WDOG_COUNT; verify ~50
4. Write 0x00000000 to WDOG_COUNT (conventional pet); read WDOG_COUNT; verify = 0
5. Advance by 30 more ticks; read WDOG_COUNT; verify ~30
6. Write 0x12345678 to WDOG_COUNT (non-zero arbitrary value pet); read WDOG_COUNT; verify = 0 (data discarded)
7. Advance by 20 more ticks; read WDOG_COUNT; verify ~20
8. Write 0xDEADBEEF to WDOG_COUNT (another arbitrary pet); read WDOG_COUNT; verify = 0
9. Verify `intr_wdog_timer_bark`, `aon_timer_rst_req` remain deasserted throughout (counter never reached bark threshold)

**Expected Results:**
- Any write to WDOG_COUNT unconditionally resets the counter to 0, regardless of the written value
- Written data is discarded; the counter always returns to 0 after a pet write
- `intr_wdog_timer_bark` and `aon_timer_rst_req` remain deasserted when counter is kept below thresholds

**Pass/Fail Criteria:** WDOG_COUNT pet works with any data value. Counter always resets to exactly 0.

---

#### TC_AON_021

**Test Name:** Watchdog Petting Under Active Bark - Counter Resets and Interrupts Clear

**Description:** Verify that petting the watchdog (writing to WDOG_COUNT) when the bark interrupt is active clears the bark condition. Verify that `intr_wdog_timer_bark`, `nmi_wdog_timer_bark`, and `wkup_req` de-assert when the counter returns to 0 (below bark threshold).

**Prerequisites:**
- Reset applied, watchdog configured with bark threshold = 5

**Test Steps:**
1. Write WDOG_BARK_THOLD = 0x00000005, WDOG_BITE_THOLD = 0xFFFFFFFF
2. Write WDOG_CTRL = 0x00000001 (enable=1)
3. Advance simulation to count=6; verify `intr_wdog_timer_bark`=1
4. Write 0x00000000 to WDOG_COUNT (pet the watchdog)
5. Read WDOG_COUNT; verify = 0
6. Immediately read INTR_STATE; check whether wdog_timer_bark bit has cleared
7. Monitor `intr_wdog_timer_bark` and `nmi_wdog_timer_bark` outputs after petting
8. Monitor `wkup_req` output after petting
9. Read WKUP_CAUSE; verify status

**Expected Results:**
- After petting (writing WDOG_COUNT), counter resets to 0
- `intr_wdog_timer_bark` and `nmi_wdog_timer_bark` de-assert because count(0) < bark threshold(5)
- `wkup_req` de-asserts if no other wakeup source is active
- WKUP_CAUSE.cause clears when `wkup_req` de-asserts

**Pass/Fail Criteria:** Watchdog petting resolves bark condition. All bark-related outputs de-assert after pet.

---

#### TC_AON_022

**Test Name:** Watchdog Sleep Pause Feature - Pause-in-Sleep Control

**Description:** Verify that the watchdog counter halts when `sleep_mode`=1 AND WDOG_CTRL.pause_in_sleep=1, resumes when `sleep_mode` de-asserts, and is unaffected by `sleep_mode` when pause_in_sleep=0.

**Prerequisites:**
- Reset applied, `lc_escalate_en`=0

**Test Steps:**
1. Write WDOG_BARK_THOLD = 0xFFFFFFFF, WDOG_BITE_THOLD = 0xFFFFFFFF
2. Write WDOG_CTRL = 0x00000003 (enable=1, pause_in_sleep=1); perform read-back
3. Advance 10 ticks; record WDOG_COUNT value A
4. Assert `sleep_mode` = 1
5. Advance 10 more ticks; read WDOG_COUNT; record value B (must equal A - paused)
6. De-assert `sleep_mode` = 0
7. Advance 10 more ticks; read WDOG_COUNT; record value C (must be approximately A+10)
8. Write WDOG_CTRL = 0x00000001 (enable=1, pause_in_sleep=0 - disabled pause); perform read-back
9. Assert `sleep_mode` = 1
10. Advance 10 ticks; read WDOG_COUNT; record value D (must have advanced - pause_in_sleep=0)
11. Verify D > C (counter counted despite sleep_mode because pause_in_sleep=0)

**Expected Results:**
- Watchdog halts only when BOTH sleep_mode=1 AND pause_in_sleep=1
- When sleep_mode de-asserts, watchdog resumes from the frozen value
- When pause_in_sleep=0, sleep_mode has no effect on watchdog counting

**Pass/Fail Criteria:** Sleep pause requires both conditions. Resume is seamless from the halted count value.

---

#### TC_AON_023

**Test Name:** Wakeup Timer Unaffected by Sleep Mode

**Description:** Verify that the wakeup timer continues counting and generates interrupts regardless of the `sleep_mode` input signal state, confirming its always-on behavior.

**Prerequisites:**
- Reset applied, `lc_escalate_en`=0

**Test Steps:**
1. Write WKUP_THOLD_LO = 0x0000000A, WKUP_THOLD_HI = 0x00000000 (threshold=10)
2. Write WKUP_COUNT to 0
3. Write WKUP_CTRL = 0x00000001 (enable=1); perform read-back
4. Assert `sleep_mode` = 1
5. Advance simulation by 5 AON ticks; read WKUP_COUNT_LO; verify it has advanced (counter still runs)
6. Advance to count=10; verify `intr_wkup_timer_expired` asserts despite `sleep_mode`=1
7. De-assert `sleep_mode` = 0; verify timer still running with interrupt asserted

**Expected Results:**
- WKUP_COUNT advances regardless of `sleep_mode` value
- `intr_wkup_timer_expired` asserts at the threshold crossing regardless of `sleep_mode`
- Wakeup timer is completely unaffected by `sleep_mode`

**Pass/Fail Criteria:** `sleep_mode` has absolutely no effect on the wakeup timer.

---

### 5.4 Interrupt Tests

---

#### TC_AON_024

**Test Name:** Wakeup Timer Interrupt via INTR_TEST Force-Assert

**Description:** Verify that writing 1 to INTR_TEST.wkup_timer_expired (bit[0]) immediately force-asserts `intr_wkup_timer_expired` and sets INTR_STATE.wkup_timer_expired without requiring timer counting or threshold crossing.

**Prerequisites:**
- Reset applied, both timers disabled

**Test Steps:**
1. Verify `intr_wkup_timer_expired` = 0 and INTR_STATE[0] = 0 before test
2. Verify WKUP_COUNT and WKUP_THOLD are at reset values (no threshold condition active)
3. Write 0x00000001 to INTR_TEST via `tl_socket`
4. Immediately read INTR_STATE via `tl_socket`; verify bit[0] = 1
5. Verify `intr_wkup_timer_expired` output = 1
6. Verify `intr_wdog_timer_bark` = 0 and `nmi_wdog_timer_bark` = 0 (unaffected)
7. Verify WKUP_COUNT and WKUP_THOLD are unchanged
8. Write 0x00000001 to INTR_STATE (W1C clear bit[0])
9. Read INTR_STATE; verify bit[0] = 0
10. Verify `intr_wkup_timer_expired` = 0

**Expected Results:**
- INTR_TEST write immediately sets INTR_STATE and asserts interrupt output
- Counter and threshold values are unaffected by INTR_TEST
- W1C on INTR_STATE correctly clears the force-asserted interrupt

**Pass/Fail Criteria:** INTR_TEST forces interrupt assertion independent of timer state.

---

#### TC_AON_025

**Test Name:** Watchdog Bark Interrupt via INTR_TEST Force-Assert and NMI Coupling

**Description:** Verify that writing 1 to INTR_TEST.wdog_timer_bark (bit[1]) simultaneously asserts `intr_wdog_timer_bark` AND `nmi_wdog_timer_bark`, and sets INTR_STATE.wdog_timer_bark. Verify NMI is driven identically to the bark interrupt at all times.

**Prerequisites:**
- Reset applied, both timers disabled

**Test Steps:**
1. Verify `intr_wdog_timer_bark` = 0, `nmi_wdog_timer_bark` = 0, INTR_STATE[1] = 0
2. Write 0x00000002 to INTR_TEST (bit[1] = wdog_timer_bark)
3. Read INTR_STATE; verify bit[1] = 1
4. Verify `intr_wdog_timer_bark` = 1
5. Verify `nmi_wdog_timer_bark` = 1 (must be identical to intr_wdog_timer_bark)
6. Verify `intr_wkup_timer_expired` = 0 (unaffected)
7. Verify WDOG_COUNT and thresholds are unchanged
8. Write 0x00000002 to INTR_STATE (W1C clear bit[1])
9. Verify `intr_wdog_timer_bark` = 0
10. Verify `nmi_wdog_timer_bark` = 0 (must clear simultaneously with intr_wdog_timer_bark)

**Expected Results:**
- Writing to INTR_TEST.wdog_timer_bark asserts both `intr_wdog_timer_bark` and `nmi_wdog_timer_bark` simultaneously
- Clearing INTR_STATE.wdog_timer_bark de-asserts both outputs simultaneously
- `nmi_wdog_timer_bark` is always identical to `intr_wdog_timer_bark`; no independent control exists

**Pass/Fail Criteria:** NMI is a wire copy of the bark interrupt at all times. Both assert and de-assert together.

---

#### TC_AON_026

**Test Name:** Wakeup Timer Interrupt Deassertion - W1C with Counter Below Threshold

**Description:** Verify the complete wakeup interrupt deassertion sequence: disable timer or reset counter below threshold, then W1C INTR_STATE to clear the interrupt. Verify that after clearing, the interrupt does NOT re-assert if the threshold condition is no longer met.

**Prerequisites:**
- Reset applied, wakeup timer triggered to set interrupt

**Test Steps:**
1. Set threshold=5, counter=0, enable timer; allow count to reach 5; verify `intr_wkup_timer_expired`=1
2. Disable timer (write WKUP_CTRL=0)
3. Write WKUP_COUNT_HI=0, WKUP_COUNT_LO=0 (reset counter to 0, below threshold)
4. Write 0x00000001 to INTR_STATE (W1C clear wkup_timer_expired)
5. Read INTR_STATE; verify bit[0]=0
6. Verify `intr_wkup_timer_expired` = 0
7. Advance simulation by 5 AON ticks (with timer still disabled)
8. Verify `intr_wkup_timer_expired` remains 0 (no re-assertion because timer is disabled)

**Expected Results:**
- After W1C clear with timer disabled and counter below threshold, interrupt de-asserts and stays de-asserted
- `wkup_req` also de-asserts if WKUP_CAUSE is cleared

**Pass/Fail Criteria:** Interrupt does not re-assert when threshold condition is not met and timer is disabled.

---

#### TC_AON_027

**Test Name:** Watchdog Bark Interrupt Deassertion Sequence

**Description:** Verify the complete bark interrupt deassertion: pet the watchdog (write to WDOG_COUNT to reset counter below bark threshold), then W1C INTR_STATE.wdog_timer_bark. Verify bark interrupt clears and does not re-assert with counter reset.

**Prerequisites:**
- Reset applied, watchdog timer triggered to bark state

**Test Steps:**
1. Configure bark threshold=5, bite threshold=0xFFFFFFFF; enable watchdog
2. Allow WDOG_COUNT to reach 6; verify `intr_wdog_timer_bark`=1, INTR_STATE[1]=1
3. Write 0x00000000 to WDOG_COUNT (pet - counter resets to 0)
4. Verify `intr_wdog_timer_bark` de-asserts (count=0 < bark threshold=5)
5. Verify `nmi_wdog_timer_bark` de-asserts simultaneously
6. Write 0x00000002 to INTR_STATE (W1C clear bit[1]) - confirm it can also be cleared via INTR_STATE
7. Read INTR_STATE; verify bit[1]=0
8. Advance 3 ticks; verify no immediate re-bark (counter at 3, below threshold 5)

**Expected Results:**
- Petting the watchdog (WDOG_COUNT write) resolves the bark condition and de-asserts bark outputs
- W1C on INTR_STATE[1] clears the stored interrupt status independently
- Bark does not re-assert if counter is below bark threshold

**Pass/Fail Criteria:** Bark interrupt fully cleared by petting watchdog and/or INTR_STATE W1C. No spurious re-assertion.

---

#### TC_AON_028

**Test Name:** Wakeup Timer Interrupt and Wakeup Request Independent Clearing

**Description:** Verify that INTR_STATE (W1C, clears interrupt path to CPU) and WKUP_CAUSE (RW0C, clears wakeup request to power manager) are independent clearing mechanisms. Clearing one does not clear the other.

**Prerequisites:**
- Reset applied, wakeup timer triggered

**Test Steps:**
1. Enable wakeup timer with threshold=3; allow count to reach 4
2. Verify `intr_wkup_timer_expired`=1, INTR_STATE[0]=1, WKUP_CAUSE[0]=1, `wkup_req`=1
3. Write 0x00000001 to INTR_STATE (W1C clear interrupt)
4. Read INTR_STATE; verify bit[0]=0 (`intr_wkup_timer_expired` de-asserted)
5. Read WKUP_CAUSE; verify bit[0]=1 still set (`wkup_req` still asserted)
6. Verify `wkup_req` output is still asserted
7. Write 0x00000000 to WKUP_CAUSE (RW0C clear wakeup request)
8. Read WKUP_CAUSE; verify bit[0]=0
9. Verify `wkup_req` output is now de-asserted

**Expected Results:**
- Clearing INTR_STATE does NOT clear WKUP_CAUSE
- `intr_wkup_timer_expired` and `wkup_req` are cleared independently
- Both must be explicitly cleared for complete event acknowledgment

**Pass/Fail Criteria:** INTR_STATE and WKUP_CAUSE are fully independent. Partial clearing leaves the other output active.

---

### 5.5 Power Management Tests

---

#### TC_AON_029

**Test Name:** Wakeup Request Persistence Until Explicit Clear

**Description:** Verify that `wkup_req` remains asserted after the threshold event until software explicitly writes 0 to WKUP_CAUSE, and that it persists across timer disable and counter manipulation.

**Prerequisites:**
- Reset applied

**Test Steps:**
1. Enable wakeup timer with threshold=3; allow count to reach threshold
2. Verify `wkup_req`=1 and WKUP_CAUSE[0]=1
3. Disable timer (WKUP_CTRL=0)
4. Read `wkup_req`; verify still=1 (persists after disable)
5. Write WKUP_COUNT_HI=0, WKUP_COUNT_LO=0 (reset counter)
6. Read `wkup_req`; verify still=1 (persists after counter reset)
7. Advance simulation 20 ticks; read `wkup_req`; verify still=1
8. Write 0x00000000 to WKUP_CAUSE (RW0C clear)
9. Read `wkup_req`; verify=0 (de-asserted after WKUP_CAUSE clear)

**Expected Results:**
- `wkup_req` persists regardless of timer enable/disable state and counter value
- Only WKUP_CAUSE write-0 or system reset de-asserts `wkup_req`

**Pass/Fail Criteria:** `wkup_req` persistence matches the architectural specification.

---

#### TC_AON_030

**Test Name:** Wakeup Request from Watchdog Bark - Dual Source to wkup_req

**Description:** Verify that `wkup_req` is asserted not only by the wakeup timer threshold crossing but also by the watchdog bark threshold crossing, confirming the dual-source nature of `wkup_req`.

**Prerequisites:**
- Reset applied, wakeup timer disabled

**Test Steps:**
1. Ensure WKUP_CTRL=0 (wakeup timer disabled, not a wakeup source)
2. Write WDOG_BARK_THOLD=5, WDOG_BITE_THOLD=0xFFFFFFFF
3. Write WDOG_CTRL=0x00000001 (watchdog enabled); perform read-back
4. Advance simulation to count=5; verify `intr_wdog_timer_bark`=1
5. Read WKUP_CAUSE; verify bit[0]=1 (wakeup set by watchdog bark)
6. Verify `wkup_req` = 1 (wakeup request asserted due to watchdog bark)
7. Verify `intr_wkup_timer_expired`=0 (wakeup timer was not involved)

**Expected Results:**
- Watchdog bark sets WKUP_CAUSE.cause=1 and asserts `wkup_req`
- `wkup_req` asserts without any wakeup timer involvement
- `intr_wkup_timer_expired` remains 0

**Pass/Fail Criteria:** `wkup_req` is driven by both timer sources. Watchdog bark alone can trigger wakeup request.

---

#### TC_AON_031

**Test Name:** Watchdog Bite Reset Request - aon_timer_rst_req Assertion

**Description:** Verify that `aon_timer_rst_req` asserts when the watchdog bite threshold is crossed, and that it persists independently of bark interrupt state.

**Prerequisites:**
- Reset applied, `lc_escalate_en`=0

**Test Steps:**
1. Write WDOG_BARK_THOLD=3, WDOG_BITE_THOLD=6
2. Write WDOG_CTRL=0x00000001 (enable=1); perform read-back
3. Advance to count=3; verify `intr_wdog_timer_bark`=1, `aon_timer_rst_req`=0
4. Advance to count=6; verify `aon_timer_rst_req`=1
5. Verify `intr_wdog_timer_bark` still=1 (bark remains active alongside bite)
6. Attempt to clear bark via INTR_STATE W1C; verify `aon_timer_rst_req` unaffected by INTR_STATE write
7. Verify `aon_timer_rst_req` is not cleared by any register write except system reset

**Expected Results:**
- `aon_timer_rst_req` asserts at bite threshold independently of bark
- `aon_timer_rst_req` is not clearable by any register write (not software-clearable)
- Bark and bite outputs are simultaneously active between bark threshold and bite threshold

**Pass/Fail Criteria:** `aon_timer_rst_req` correctly asserts at bite threshold and cannot be cleared by software.

---

### 5.6 Security Tests

---

#### TC_AON_032

**Test Name:** WDOG_REGWEN Lock - Protected Registers Silently Ignore Writes

**Description:** Verify that after locking WDOG_REGWEN (writing 0), writes to WDOG_CTRL, WDOG_BARK_THOLD, and WDOG_BITE_THOLD are silently discarded. Bus transactions complete normally without error, but register values do not change.

**Prerequisites:**
- Reset applied, watchdog configured with known values before locking

**Test Steps:**
1. Write WDOG_CTRL=0x00000001, WDOG_BARK_THOLD=0x00001000, WDOG_BITE_THOLD=0x00002000
2. Perform read-backs to confirm written values
3. Write 0x00000000 to WDOG_REGWEN (lock the watchdog configuration)
4. Read WDOG_REGWEN; verify=0x00000000 (locked)
5. Attempt write 0x00000002 to WDOG_CTRL (change pause_in_sleep)
6. Perform read-back of WDOG_CTRL; verify value unchanged (still 0x00000001)
7. Attempt write 0x0000FFFF to WDOG_BARK_THOLD
8. Read WDOG_BARK_THOLD; verify unchanged (still 0x00001000)
9. Attempt write 0x0000FFFF to WDOG_BITE_THOLD
10. Read WDOG_BITE_THOLD; verify unchanged (still 0x00002000)
11. Verify no error response from `tl_socket` for any of the locked writes (transactions complete normally)

**Expected Results:**
- Writes to WDOG_CTRL, WDOG_BARK_THOLD, WDOG_BITE_THOLD are silently ignored when WDOG_REGWEN=0
- No error response is generated; bus transactions complete normally
- Register values remain unchanged from pre-lock configuration

**Pass/Fail Criteria:** Lock protection works silently. No error responses. Register values frozen.

---

#### TC_AON_033

**Test Name:** WDOG_REGWEN Lock - WDOG_COUNT Remains Writable (Petting Always Allowed)

**Description:** Verify that WDOG_COUNT is NOT gated by WDOG_REGWEN. After locking the watchdog configuration, software must still be able to pet the watchdog by writing to WDOG_COUNT.

**Prerequisites:**
- Reset applied

**Test Steps:**
1. Write WDOG_BARK_THOLD=20, WDOG_BITE_THOLD=0xFFFFFFFF
2. Write WDOG_CTRL=0x00000001 (enable=1)
3. Write 0x00000000 to WDOG_REGWEN (lock configuration)
4. Advance simulation by 15 ticks; read WDOG_COUNT; verify ~15
5. Write 0x00000000 to WDOG_COUNT (pet the watchdog - must work despite lock)
6. Read WDOG_COUNT; verify = 0 (pet was successful)
7. Verify no error response from `tl_socket` for the WDOG_COUNT write
8. Verify `intr_wdog_timer_bark` remains 0 (counter reset before reaching bark threshold)

**Expected Results:**
- WDOG_COUNT write succeeds despite WDOG_REGWEN=0
- Counter resets to 0 after any write to WDOG_COUNT
- Watchdog petting is always possible regardless of configuration lock state

**Pass/Fail Criteria:** WDOG_COUNT petting works while configuration is locked. Lock does not affect WDOG_COUNT.

---

#### TC_AON_034

**Test Name:** ALERT_TEST Fatal Fault Alert Connectivity Test

**Description:** Verify that writing 1 to ALERT_TEST.fatal_fault asserts the `fatal_fault` output signal as a transient pulse. Verify the alert does not persist after the write. Verify reserved bits in ALERT_TEST are ignored.

**Prerequisites:**
- Reset applied

**Test Steps:**
1. Verify `fatal_fault` output = 0 before test
2. Write 0x00000001 to ALERT_TEST via `tl_socket`
3. Monitor `fatal_fault` output during write; verify it asserts
4. Advance simulation by a few time units; verify `fatal_fault` de-asserts (transient pulse)
5. Read ALERT_TEST; verify = 0x00000000 (no storage)
6. Verify INTR_STATE and all other status registers are unchanged by ALERT_TEST write
7. Write 0xFFFFFFFE to ALERT_TEST (reserved bits set, bit[0]=0)
8. Verify `fatal_fault` does NOT assert (only bit[0]=1 triggers alert)

**Expected Results:**
- `fatal_fault` asserts transiently when bit[0] of ALERT_TEST is written as 1
- `fatal_fault` does not persist after the write pulse
- Reads of ALERT_TEST always return 0x00000000
- Writing bit[0]=0 does not assert `fatal_fault`

**Pass/Fail Criteria:** ALERT_TEST correctly triggers a transient `fatal_fault` pulse for bit[0]=1 write.

---

#### TC_AON_035

**Test Name:** RACL Access Control with EnableRacl=1 - Policy Enforcement

**Description:** Verify that when `EnableRacl=1`, unauthorized register access attempts result in the access being blocked and `racl_error` reporting the violation. Verify that authorized accesses proceed normally.

**Prerequisites:**
- Model instantiated with `EnableRacl=1`
- `racl_policies` port driven with test policy vectors
- Reset applied

**Test Steps:**
1. Drive `racl_policies` with a policy vector that denies write access to WKUP_CTRL
2. Attempt a write transaction to WKUP_CTRL via `tl_socket`
3. Monitor `racl_error` output; verify it reports an access violation
4. Read WKUP_CTRL; verify value unchanged (write was blocked)
5. Drive `racl_policies` with a policy vector that grants write access to WKUP_CTRL
6. Attempt a write transaction to WKUP_CTRL
7. Read WKUP_CTRL; verify write succeeded
8. Verify `racl_error` did not assert for the authorized write
9. Drive `racl_policies` with a policy vector that denies read access to WDOG_COUNT
10. Attempt a read transaction to WDOG_COUNT; monitor `racl_error`

**Expected Results:**
- Unauthorized accesses are blocked and reported via `racl_error`
- Authorized accesses proceed normally without `racl_error` assertion
- `racl_error` provides violation information for unauthorized accesses

**Pass/Fail Criteria:** RACL policy enforcement correctly gates register access. `racl_error` fires only for violations.

---

#### TC_AON_036

**Test Name:** RACL Absent with EnableRacl=0 - No RACL Ports Present

**Description:** Verify that when `EnableRacl=0` (default), the `racl_policies` and `racl_error` ports are absent from the module interface and no RACL enforcement is applied. Register accesses proceed with only standard TL-UL and WDOG_REGWEN access control.

**Prerequisites:**
- Model instantiated with `EnableRacl=0` (default)
- Reset applied

**Test Steps:**
1. Confirm that `racl_policies` and `racl_error` ports do not exist in the model interface
2. Perform write and read transactions to all 14 registers via `tl_socket`
3. Verify all accesses complete normally without any RACL-related errors
4. Verify WDOG_REGWEN lock still functions (non-RACL access control)

**Expected Results:**
- No `racl_policies` input and no `racl_error` output exist when EnableRacl=0
- All register accesses succeed without RACL interference
- Standard TL-UL and WDOG_REGWEN access control still applies

**Pass/Fail Criteria:** When EnableRacl=0, RACL infrastructure is absent and no access control enforcement beyond WDOG_REGWEN is applied.

---

### 5.7 64-bit Non-Atomic Access Tests

---

#### TC_AON_037

**Test Name:** WKUP_COUNT 64-bit Safe Read Using Double-Read Technique

**Description:** Verify that the safe double-read technique correctly assembles the 64-bit wakeup counter value even when the LO register overflows between reads. Read HI, then LO, then HI again; if second HI differs from first, re-read LO.

**Prerequisites:**
- Reset applied, wakeup timer enabled and counting with threshold set to maximum

**Test Steps:**
1. Set WKUP_THOLD to maximum (0xFFFFFFFF_FFFFFFFF)
2. Load WKUP_COUNT to just before LO overflow: HI=0x00000001, LO=0xFFFFFFFE
3. Enable timer (prescaler=0)
4. Read WKUP_COUNT_HI (call it HI_first); record simulation time T1
5. Advance simulation by 2 ticks (allows LO overflow to occur)
6. Read WKUP_COUNT_LO (call it LO); record value
7. Read WKUP_COUNT_HI (call it HI_second); record value
8. If HI_first != HI_second: re-read WKUP_COUNT_LO using HI_second; this is the correct LO
9. Assemble 64-bit value = (HI_second << 32) | corrected_LO
10. Verify assembled 64-bit value is consistent with simulation time elapsed

**Expected Results:**
- When LO overflows between reads, HI_first != HI_second is detected
- The double-read correction produces a consistent 64-bit value
- The naive assembly (HI_first:LO) would have been incorrect (off by 2^32)

**Pass/Fail Criteria:** Double-read technique provides a correct and consistent 64-bit counter value even across LO overflow.

---

#### TC_AON_038

**Test Name:** WKUP_COUNT 64-bit Safe Write - Disable Timer Before Writing HI and LO

**Description:** Verify that writing both WKUP_COUNT_HI and WKUP_COUNT_LO while the timer is disabled results in the counter being set to the intended 64-bit value without a race condition. Verify that writing counter registers while the timer is enabled risks race conditions.

**Prerequisites:**
- Reset applied

**Test Steps:**
1. Set WKUP_THOLD to maximum; enable timer at prescaler=0
2. Observe that WDOG_COUNT increments continuously
3. Without disabling the timer: write WKUP_COUNT_HI=0x00000000; then immediately write WKUP_COUNT_LO=0x00000001
4. Read back both registers; note that the assembled 64-bit value may differ from the intended value due to race
5. Now perform the safe write: write WKUP_CTRL=0x00000000 (disable timer)
6. Write WKUP_COUNT_HI=0x00000005, WKUP_COUNT_LO=0x00000000 (set to 0x00000005_00000000)
7. Read back HI and LO immediately; verify HI=0x00000005, LO=0x00000000 (no race possible while disabled)
8. Write WKUP_CTRL=0x00000001 (re-enable); perform read-back
9. Read count again after 3 ticks; verify count advanced from 0x00000005_00000000

**Expected Results:**
- Safe write sequence (disable, write HI, write LO, re-enable) produces the correct starting counter value
- Counter advances from the written value after re-enabling

**Pass/Fail Criteria:** Safe write sequence sets counter to the exact intended 64-bit value with no race condition.

---

#### TC_AON_039

**Test Name:** WKUP_THOLD 64-bit Safe Write - Spurious Interrupt Prevention Sequence

**Description:** Verify the safe three-step threshold write sequence (set LO to 0xFFFFFFFF, write new HI, write new LO) prevents spurious wakeup interrupts from the transient intermediate threshold state.

**Prerequisites:**
- Reset applied, wakeup timer enabled and counting near the old threshold

**Test Steps:**
1. Set initial threshold: WKUP_THOLD_HI=0x00000001, WKUP_THOLD_LO=0x00000000
2. Set counter to 0, enable timer; allow counting to proceed
3. Decide to change threshold to 0x00000002_00000000
4. Perform safe threshold write: write WKUP_THOLD_LO=0xFFFFFFFF first
5. Write WKUP_THOLD_HI=0x00000002
6. Write WKUP_THOLD_LO=0x00000000 (complete the new threshold)
7. Verify no spurious `intr_wkup_timer_expired` assertion occurred during the intermediate state (when HI=0x00000002, LO=0xFFFFFFFF, the 64-bit threshold = 0x00000002_FFFFFFFF which is > counter value)
8. Allow counter to advance to 0x00000002_00000000; verify interrupt asserts correctly

**Expected Results:**
- The intermediate threshold state (after LO=0xFFFFFFFF write, before new HI) must be >= old threshold to prevent spurious interrupt
- The safe write sequence prevents any spurious interrupt assertion
- Final threshold write completes with correct threshold and interrupt fires at the new threshold value

**Pass/Fail Criteria:** No spurious interrupt during threshold update. Interrupt fires correctly at new threshold.

---

#### TC_AON_040

**Test Name:** WKUP_THOLD 64-bit Read - Sequential Read is Race-Condition Free

**Description:** Verify that reading WKUP_THOLD_HI and WKUP_THOLD_LO sequentially is always safe because hardware does not modify threshold registers. No double-read technique is required for threshold reads.

**Prerequisites:**
- Reset applied, wakeup timer enabled and counting

**Test Steps:**
1. Write WKUP_THOLD_HI=0xABCDEF01, WKUP_THOLD_LO=0x23456789 (set known threshold values)
2. Enable timer with prescaler=0
3. Read WKUP_THOLD_HI; record value
4. Advance simulation by 100 ticks
5. Read WKUP_THOLD_LO; record value
6. Verify WKUP_THOLD_HI=0xABCDEF01 and WKUP_THOLD_LO=0x23456789 (unchanged)
7. Read WKUP_THOLD_HI again; verify identical to first read

**Expected Results:**
- WKUP_THOLD_HI and WKUP_THOLD_LO never change due to hardware activity
- Sequential threshold reads are always consistent
- No double-read technique is needed for threshold registers

**Pass/Fail Criteria:** Threshold registers are immutable by hardware; consecutive reads always return the same software-written value.

---

### 5.8 Reset Behavior Tests

---

#### TC_AON_041

**Test Name:** System Reset (rst_n) Clears All Counters, Thresholds, and Outputs

**Description:** Verify that asserting `rst_n` (system reset) restores all 14 registers to their documented reset values, de-asserts all output signals, and restores WDOG_REGWEN to unlocked state even after it was locked.

**Prerequisites:**
- Pre-configure model in a complex active state before reset

**Test Steps:**
1. Configure both timers with non-default threshold and counter values
2. Enable both timers and allow threshold crossings to assert interrupts and wakeup signals
3. Lock watchdog configuration (write 0 to WDOG_REGWEN)
4. Assert `sleep_mode`=1
5. Record all output signal states (should be: `intr_wkup_timer_expired`=1, `intr_wdog_timer_bark`=1, `nmi_wdog_timer_bark`=1, `wkup_req`=1, WDOG_REGWEN=0)
6. Assert `rst_n`=0 and `rst_aon_n`=0 (apply system reset)
7. Wait for reset propagation
8. De-assert `rst_n`=1 and `rst_aon_n`=1
9. Read all 14 registers; verify reset values (refer to Section 3 Register Map Reference)
10. Verify all output signals: `intr_wkup_timer_expired`=0, `intr_wdog_timer_bark`=0, `nmi_wdog_timer_bark`=0, `wkup_req`=0, `aon_timer_rst_req`=0
11. Verify WDOG_REGWEN = 0x00000001 (lock cleared by reset)

**Expected Results:**
- All registers at documented reset values after system reset
- All output signals de-asserted
- WDOG_REGWEN restored to 0x00000001 (unlocked)

**Pass/Fail Criteria:** Complete state restoration after system reset. Lock cleared. All outputs de-asserted.

---

#### TC_AON_042

**Test Name:** Watchdog Bite Induced Reset - aon_timer_rst_req Triggers Reset Sequence

**Description:** Verify that when `aon_timer_rst_req` asserts due to a watchdog bite, the power manager's subsequent system reset (simulated as `rst_n` assertion) restores all AON Timer state. Verify no software acknowledgment is required.

**Prerequisites:**
- Reset applied, watchdog configured for bite test

**Test Steps:**
1. Write WDOG_BARK_THOLD=3, WDOG_BITE_THOLD=5
2. Write WDOG_CTRL=0x00000001 (enable=1); perform read-back
3. Advance simulation to count=5; verify `aon_timer_rst_req`=1
4. Simulate power manager response: assert `rst_n`=0 and `rst_aon_n`=0
5. Wait for reset settling
6. De-assert `rst_n`=1 and `rst_aon_n`=1
7. Read all registers; verify all at reset state including WDOG_REGWEN=0x00000001
8. Verify `aon_timer_rst_req`=0 (de-asserted after reset)
9. Verify WDOG_COUNT=0, WDOG_CTRL=0 (timer disabled after reset)

**Expected Results:**
- After watchdog-bite-induced system reset, all AON Timer registers restore to reset values
- No software writes are needed to clear the bite reset state
- Timer is disabled after reset; watchdog configuration lock is removed

**Pass/Fail Criteria:** Watchdog bite reset results in clean state restoration without software intervention.

---

#### TC_AON_043

**Test Name:** Independent AON Domain Reset via rst_aon_n

**Description:** Verify that asserting `rst_aon_n` (AON domain reset) clears AON-domain outputs (`wkup_req` and `aon_timer_rst_req`) and AON domain register state, while the SYS domain remains in its current state.

**Prerequisites:**
- Reset applied, both timers running with thresholds crossed

**Test Steps:**
1. Enable wakeup timer and watchdog; configure thresholds to trigger both wakeup and bark outputs
2. Verify `wkup_req`=1, WKUP_CAUSE[0]=1, `intr_wkup_timer_expired`=1
3. Assert `rst_aon_n`=0 while keeping `rst_n`=1
4. Wait for reset propagation
5. De-assert `rst_aon_n`=1
6. Read WKUP_CAUSE; verify bit[0]=0 (`wkup_req` cleared by AON reset)
7. Verify `wkup_req`=0 (AON domain output cleared)
8. Verify `aon_timer_rst_req`=0 (AON domain output cleared)

**Expected Results:**
- `rst_aon_n` clears AON-domain outputs and state (`wkup_req`, `aon_timer_rst_req`, WKUP_CAUSE)
- AON reset restores AON-domain registers to their reset values

**Pass/Fail Criteria:** `rst_aon_n` clears AON domain state correctly.

---

### 5.9 Escalation Tests

---

#### TC_AON_044

**Test Name:** Lifecycle Escalation Halts Both Wakeup and Watchdog Counters Immediately

**Description:** Verify that asserting `lc_escalate_en` immediately freezes both the 64-bit wakeup counter and the 32-bit watchdog counter, regardless of their enable state, overriding all other control logic.

**Prerequisites:**
- Reset applied, both timers enabled and counting

**Test Steps:**
1. Set WKUP_THOLD and WDOG bark/bite thresholds to maximum to prevent threshold crossings
2. Enable both timers: WKUP_CTRL=0x00000001, WDOG_CTRL=0x00000001; perform read-backs
3. Advance simulation by 10 ticks; record WKUP_COUNT_LO value A and WDOG_COUNT value B
4. Assert `lc_escalate_en`=1
5. Advance simulation by 20 more ticks
6. Read WKUP_COUNT_LO; record value C (must equal A - counter frozen)
7. Read WDOG_COUNT; record value D (must equal B - counter frozen)
8. De-assert `lc_escalate_en`=0
9. Advance simulation by 10 more ticks
10. Read WKUP_COUNT_LO; record value E (must be approximately C+10)
11. Read WDOG_COUNT; record value F (must be approximately D+10)

**Expected Results:**
- Value C = Value A (wakeup counter frozen during escalation)
- Value D = Value B (watchdog counter frozen during escalation)
- After de-asserting `lc_escalate_en`, both counters resume from their frozen values
- Value E >= A+10, Value F >= B+10 (resumed counting)

**Pass/Fail Criteria:** Both counters freeze immediately on escalation and resume correctly after de-escalation.

---

#### TC_AON_045

**Test Name:** Escalation During Active Threshold Condition - Interrupts Persist

**Description:** Verify that when `lc_escalate_en` is asserted while interrupts are active (counter already above threshold), the existing interrupt signals remain asserted in their current state. The counter is frozen and does not advance further.

**Prerequisites:**
- Reset applied, wakeup timer configured and triggered

**Test Steps:**
1. Configure wakeup timer: threshold=5, counter=0; enable timer
2. Advance simulation until count=7 (above threshold); verify `intr_wkup_timer_expired`=1
3. Assert `lc_escalate_en`=1 (escalation while interrupt is active)
4. Read WKUP_COUNT_LO; record frozen value (must be ~7)
5. Advance simulation 20 ticks; read WKUP_COUNT_LO again; verify unchanged (frozen)
6. Read INTR_STATE; verify bit[0] still=1 (interrupt persists - escalation does not clear it)
7. Verify `intr_wkup_timer_expired` output still=1
8. De-assert `lc_escalate_en`=0
9. Advance 1 tick; verify counter resumes and interrupt may re-assert (count still >= threshold)

**Expected Results:**
- Escalation freezes counter but does NOT clear pending interrupts
- Previously asserted outputs (`intr_wkup_timer_expired`, `wkup_req`) remain in asserted state
- After escalation is released, the timer resumes and re-evaluates threshold condition

**Pass/Fail Criteria:** Escalation preserves existing interrupt state while freezing counter advancement.

---

#### TC_AON_046

**Test Name:** Escalation Prevents Watchdog Bite During Escalation Processing

**Description:** Verify that when `lc_escalate_en` is asserted with the watchdog counter below the bite threshold, the watchdog counter remains frozen and `aon_timer_rst_req` does NOT assert during escalation, preventing unintended watchdog bites during security operations.

**Prerequisites:**
- Reset applied, watchdog timer running

**Test Steps:**
1. Write WDOG_BARK_THOLD=100, WDOG_BITE_THOLD=150
2. Enable watchdog: WDOG_CTRL=0x00000001; perform read-back
3. Advance simulation to count=120 (above bark=100, below bite=150); verify `intr_wdog_timer_bark`=1
4. Assert `lc_escalate_en`=1 (freeze counter at 120)
5. Advance simulation 100 more ticks
6. Verify WDOG_COUNT remains at ~120 (frozen)
7. Verify `aon_timer_rst_req`=0 (bite not triggered despite long elapsed time)
8. De-assert `lc_escalate_en`=0
9. Advance 31 more ticks; verify `aon_timer_rst_req` eventually asserts when count reaches 150

**Expected Results:**
- During escalation, counter is frozen and cannot reach bite threshold
- `aon_timer_rst_req` does not assert while `lc_escalate_en`=1
- After de-escalation, counter resumes and bite threshold is eventually reached

**Pass/Fail Criteria:** Escalation prevents watchdog bite while active. Bite occurs correctly after de-escalation when counter reaches threshold.

---

#### TC_AON_047

**Test Name:** Escalation Does Not Affect Register Read/Write Access

**Description:** Verify that `lc_escalate_en` freezes counter increment events but does NOT prevent software from reading or writing registers via `tl_socket`. Register access must remain functional during escalation.

**Prerequisites:**
- Reset applied, both timers enabled

**Test Steps:**
1. Enable both timers; assert `lc_escalate_en`=1
2. Attempt write 0x00000002 to WDOG_CTRL via `tl_socket` (change pause_in_sleep)
3. Verify `tl_socket` returns a successful response (no bus error)
4. Read WDOG_CTRL; verify value updated to 0x00000002
5. Write to WKUP_THOLD_HI via `tl_socket`; verify success
6. Read INTR_STATE; verify read completes successfully
7. Write 0x00000000 to WDOG_COUNT (pet watchdog during escalation); verify counter resets to 0
8. Verify all register accesses complete without error throughout escalation

**Expected Results:**
- `lc_escalate_en` does not prevent register reads or writes
- Bus transactions complete normally during escalation
- Watchdog petting (WDOG_COUNT write) works during escalation

**Pass/Fail Criteria:** Escalation affects only counter increment events, not register bus access.

---

### 5.10 Edge and Corner Case Tests

---

#### TC_AON_048

**Test Name:** Counter Initialized Above Threshold - Immediate Interrupt on Enable

**Description:** Verify that if the counter is initialized to a value at or above the threshold and then the timer is enabled, the interrupt asserts immediately on the first AON clock tick without requiring any counting.

**Prerequisites:**
- Reset applied

**Test Steps:**
1. Write WKUP_THOLD_LO=0x00000005, WKUP_THOLD_HI=0x00000000
2. Write WKUP_COUNT_HI=0x00000000, WKUP_COUNT_LO=0x00000008 (counter above threshold)
3. Verify `intr_wkup_timer_expired`=0 before enable (timer disabled, no interrupt)
4. Write WKUP_CTRL=0x00000001 (enable=1); perform read-back
5. Advance simulation by 1 AON tick
6. Verify `intr_wkup_timer_expired`=1 (asserted immediately after enable with count above threshold)
7. Read INTR_STATE; verify bit[0]=1
8. Read WKUP_CAUSE; verify bit[0]=1

**Expected Results:**
- Timer enable with counter >= threshold causes immediate interrupt and wakeup request after the first tick
- Software must be prepared to service the interrupt immediately upon enabling a timer with pre-loaded counter above threshold

**Pass/Fail Criteria:** Interrupt asserts on the first AON tick after enable when counter starts at or above threshold.

---

#### TC_AON_049

**Test Name:** Watchdog Bark and Bite at Same Threshold - Simultaneous Assertion

**Description:** Verify that when WDOG_BARK_THOLD equals WDOG_BITE_THOLD, both bark interrupt and bite reset request assert simultaneously at the same count value, without any warning interval.

**Prerequisites:**
- Reset applied, `lc_escalate_en`=0

**Test Steps:**
1. Write WDOG_BARK_THOLD=0x00000005, WDOG_BITE_THOLD=0x00000005 (equal thresholds)
2. Write WDOG_COUNT=0x00000000, WDOG_CTRL=0x00000001 (enable=1); perform read-back
3. Advance simulation one tick at a time; monitor `intr_wdog_timer_bark` and `aon_timer_rst_req`
4. At count=4: verify both outputs = 0
5. At count=5: verify `intr_wdog_timer_bark`=1 AND `aon_timer_rst_req`=1 simultaneously
6. Verify `nmi_wdog_timer_bark`=1 simultaneously
7. Verify `wkup_req`=1 simultaneously

**Expected Results:**
- Both bark and bite assert simultaneously at count = threshold when bark threshold = bite threshold
- No warning interval exists (bite fires with bark)

**Pass/Fail Criteria:** Equal bark and bite thresholds result in simultaneous assertion of both outputs.

---

#### TC_AON_050

**Test Name:** Watchdog Bite Threshold Lower Than Bark - Bite Before Bark

**Description:** Verify that when WDOG_BITE_THOLD is set lower than WDOG_BARK_THOLD, the bite reset request asserts before the bark interrupt, and no bark warning is delivered before the system reset.

**Prerequisites:**
- Reset applied

**Test Steps:**
1. Write WDOG_BARK_THOLD=0x0000000A (bark at 10), WDOG_BITE_THOLD=0x00000005 (bite at 5)
2. Write WDOG_COUNT=0x00000000, WDOG_CTRL=0x00000001 (enable=1); perform read-back
3. Advance to count=5; verify `aon_timer_rst_req`=1
4. Verify `intr_wdog_timer_bark`=0 at count=5 (bark threshold not yet reached)
5. Continue advancing to count=10; verify `intr_wdog_timer_bark` now also=1
6. Verify `aon_timer_rst_req` was asserted before `intr_wdog_timer_bark`

**Expected Results:**
- `aon_timer_rst_req` asserts at count=5 (bite threshold)
- `intr_wdog_timer_bark` asserts later at count=10 (bark threshold)
- Bite occurs without bark warning when bite threshold < bark threshold

**Pass/Fail Criteria:** Bite precedes bark when bite threshold is lower. No bark warning is delivered before bite.

---

#### TC_AON_051

**Test Name:** Watchdog Zero-Value Bite Threshold - Immediate Bite on Enable

**Description:** Verify that setting WDOG_BITE_THOLD=0 and enabling the watchdog causes `aon_timer_rst_req` to assert immediately on the first AON tick, since count(0) >= threshold(0).

**Prerequisites:**
- Reset applied

**Test Steps:**
1. Write WDOG_BARK_THOLD=0x00000000, WDOG_BITE_THOLD=0x00000000 (zero thresholds)
2. Write WDOG_COUNT=0x00000000 (counter at 0)
3. Write WDOG_CTRL=0x00000001 (enable=1); perform read-back
4. Advance simulation by 1 AON tick
5. Verify `aon_timer_rst_req`=1 (count=0 >= bite threshold=0)
6. Verify `intr_wdog_timer_bark`=1 (count=0 >= bark threshold=0)

**Expected Results:**
- With zero thresholds and counter starting at 0, both bark and bite assert on the first AON tick after enable
- Software must configure non-zero thresholds before enabling the watchdog to avoid immediate triggering

**Pass/Fail Criteria:** Zero-threshold watchdog fires immediately on the first AON tick after enable.

---

#### TC_AON_052

**Test Name:** 64-bit Wakeup Counter Overflow Wrap-Around

**Description:** Verify that the 64-bit wakeup counter wraps around from the maximum value (0xFFFFFFFF_FFFFFFFF) to 0x00000000_00000000 and continues counting, without saturation or error.

**Prerequisites:**
- Reset applied

**Test Steps:**
1. Set WKUP_THOLD to maximum (0xFFFFFFFF_FFFFFFFF) to prevent threshold crossing
2. Load WKUP_COUNT: HI=0xFFFFFFFF, LO=0xFFFFFFFE (two counts before maximum)
3. Enable timer: WKUP_CTRL=0x00000001; perform read-back
4. Advance simulation by 1 tick; read counter; verify HI=0xFFFFFFFF, LO=0xFFFFFFFF
5. Advance by 1 more tick; read counter; verify HI=0x00000000, LO=0x00000000 (wrapped to 0)
6. Verify no error assertion, no `intr_wkup_timer_expired` (threshold not crossed by the wrap)
7. Advance 3 more ticks; verify counter continues advancing from 1, 2, 3

**Expected Results:**
- Counter wraps from maximum to zero without saturation or error
- Counting continues normally after wrap-around
- No spurious interrupt or alert from the counter overflow

**Pass/Fail Criteria:** 64-bit counter wraps cleanly from 0xFFFFFFFF_FFFFFFFF to 0x00000000_00000000.

---

#### TC_AON_053

**Test Name:** 32-bit Watchdog Counter Overflow Wrap-Around

**Description:** Verify that the 32-bit watchdog counter wraps around from 0xFFFFFFFF to 0x00000000 and continues counting, without saturation or error.

**Prerequisites:**
- Reset applied

**Test Steps:**
1. Set WDOG_BARK_THOLD=0xFFFFFFFF and WDOG_BITE_THOLD=0xFFFFFFFF (maximum thresholds to allow wrap test)
2. Write WDOG_COUNT=0xFFFFFFFE (two counts before maximum)
3. Enable watchdog: WDOG_CTRL=0x00000001; perform read-back
4. Advance 1 tick; read WDOG_COUNT; verify=0xFFFFFFFF; verify bark asserts (count=BARK_THOLD=0xFFFFFFFF)
5. Advance 1 more tick; read WDOG_COUNT; verify=0x00000000 (wrapped)
6. Verify `intr_wdog_timer_bark` and `aon_timer_rst_req` behavior after wrap (count < threshold, but level interrupt depends on prior INTR_STATE)
7. Advance 5 more ticks; verify counter at 5 and continuing

**Expected Results:**
- 32-bit watchdog counter wraps from 0xFFFFFFFF to 0x00000000 cleanly
- Counter continues advancing from 0 after wrap
- No error or halt condition from counter wrap

**Pass/Fail Criteria:** 32-bit counter wraps cleanly from 0xFFFFFFFF to 0x00000000.

---

#### TC_AON_054

**Test Name:** Interrupt Re-Assertion Storm - Disabled Timer Prevents Re-Assertion

**Description:** Verify that when INTR_STATE is cleared via W1C but the timer is disabled before the next AON tick, the interrupt does NOT re-assert (no interrupt storm condition). Confirm that re-assertion only occurs when the threshold condition persists AND the timer is enabled.

**Prerequisites:**
- Reset applied

**Test Steps:**
1. Configure wakeup timer: threshold=5, counter=0; enable timer
2. Allow count to reach 7 (above threshold); verify `intr_wkup_timer_expired`=1
3. Disable timer: write WKUP_CTRL=0x00000000
4. Write 0x00000001 to INTR_STATE (W1C clear interrupt)
5. Verify INTR_STATE[0]=0 and `intr_wkup_timer_expired`=0
6. Advance simulation 10 ticks; verify `intr_wkup_timer_expired` remains 0 (no re-assertion with timer disabled)
7. Re-enable timer: write WKUP_CTRL=0x00000001 (count still at 7, above threshold 5)
8. Advance 1 tick; verify `intr_wkup_timer_expired` re-asserts (timer re-enabled with count >= threshold)

**Expected Results:**
- No interrupt re-assertion when timer is disabled, even with count >= threshold
- Interrupt re-asserts when timer is re-enabled with count still at or above threshold
- Disable/W1C is an effective way to suppress re-triggering

**Pass/Fail Criteria:** Re-assertion requires both threshold condition and timer enable=1.

---

#### TC_AON_055

**Test Name:** Both Timers Running Concurrently and Independently

**Description:** Verify that the wakeup timer and watchdog timer operate completely independently. Wakeup timer prescaler changes do not affect the watchdog counter rate, and watchdog state changes do not affect the wakeup counter.

**Prerequisites:**
- Reset applied

**Test Steps:**
1. Set WKUP_THOLD to maximum, WDOG thresholds to maximum (prevent crossings)
2. Enable wakeup timer with prescaler=3: WKUP_CTRL=0x00000007 (prescaler=3, enable=1); perform read-back
3. Enable watchdog: WDOG_CTRL=0x00000001; perform read-back
4. Advance simulation by 4 AON ticks
5. Read WKUP_COUNT_LO; verify = 1 (prescaler=3: one count per 4 ticks)
6. Read WDOG_COUNT; verify = 4 (no prescaler: one count per tick)
7. Disable watchdog: WDOG_CTRL=0x00000000; perform read-back
8. Advance 4 more ticks; read WKUP_COUNT_LO; verify = 2 (wakeup timer still advancing at same rate)
9. Read WDOG_COUNT; verify unchanged (watchdog disabled)

**Expected Results:**
- Wakeup counter advances at prescaler-divided rate independently of watchdog state
- Watchdog counter advances at AON clock rate independently of wakeup timer settings
- Disabling one timer has no effect on the other timer's counting

**Pass/Fail Criteria:** Both timers operate with full independence; no cross-interference between them.

---

#### TC_AON_056

**Test Name:** WKUP_CTRL Same-Value Write Still Resets Prescaler Accumulator

**Description:** Verify that writing the same value to WKUP_CTRL (no field change) still resets the internal prescaler accumulator, observable as the counter not advancing for a full prescaler period after the write.

**Prerequisites:**
- Reset applied, wakeup timer running with prescaler=9

**Test Steps:**
1. Set WKUP_THOLD to maximum; write WKUP_CTRL=0x00000013 (prescaler=9, enable=1); perform read-back
2. Advance simulation by 5 AON ticks; read WKUP_COUNT_LO (count should be 0 since 10 ticks needed per count)
3. Write WKUP_CTRL=0x00000013 (exact same value again); perform read-back
4. Advance simulation by 5 AON ticks; read WKUP_COUNT_LO; verify still 0 (prescaler reset at step 3 - need 10 more ticks)
5. Advance 5 more ticks (15 total since step 3 write - but only 10 needed so count should be 1)
6. Read WKUP_COUNT_LO; verify = 1 (prescaler period starts fresh from the last WKUP_CTRL write)

**Expected Results:**
- Same-value write to WKUP_CTRL resets the prescaler accumulator
- The 5 elapsed ticks before the redundant write are discarded
- First count occurs 10 ticks after the same-value write

**Pass/Fail Criteria:** Every write to WKUP_CTRL, even with unchanged value, resets prescaler accumulator.

---

#### TC_AON_057

**Test Name:** Wakeup Request from WKUP_CAUSE Not Set After Only INTR_STATE Clear

**Description:** Verify the independence of INTR_STATE and WKUP_CAUSE. After clearing the interrupt via INTR_STATE (W1C), the wakeup request path through WKUP_CAUSE must still be cleared separately. Software that clears only INTR_STATE leaves `wkup_req` active.

**Prerequisites:**
- Reset applied, wakeup timer triggered

**Test Steps:**
1. Enable wakeup timer; trigger threshold crossing; verify INTR_STATE[0]=1 and WKUP_CAUSE[0]=1
2. Clear only INTR_STATE: write 0x00000001 to INTR_STATE (W1C)
3. Read INTR_STATE; verify bit[0]=0 (`intr_wkup_timer_expired` cleared)
4. Read WKUP_CAUSE; verify bit[0]=1 (wakeup cause NOT cleared)
5. Verify `wkup_req` = 1 (still active)
6. Now clear WKUP_CAUSE: write 0x00000000 to WKUP_CAUSE (RW0C)
7. Read WKUP_CAUSE; verify bit[0]=0
8. Verify `wkup_req` = 0 (now de-asserted)

**Expected Results:**
- Clearing INTR_STATE does not affect WKUP_CAUSE
- `wkup_req` persists until WKUP_CAUSE is explicitly cleared
- Full event acknowledgment requires both INTR_STATE (W1C) and WKUP_CAUSE (RW0C) writes

**Pass/Fail Criteria:** The two clearing mechanisms are fully independent. Both must be used for complete acknowledgment.

---

#### TC_AON_058

**Test Name:** Maximum Prescaler Value - Wakeup Timer Minimum Rate

**Description:** Verify that the maximum prescaler value of 4095 produces the minimum counting rate, with the wakeup counter incrementing once every 4096 AON ticks.

**Prerequisites:**
- Reset applied

**Test Steps:**
1. Set WKUP_THOLD to maximum (prevent threshold crossing)
2. Write WKUP_COUNT to 0; Write WKUP_CTRL=0x00001FFF (prescaler=0xFFF=4095, enable=1); perform read-back
3. Advance simulation by 4095 AON ticks; read WKUP_COUNT_LO; verify = 0 (4096 ticks needed for first increment)
4. Advance simulation by 1 more AON tick (total 4096); read WKUP_COUNT_LO; verify = 1

**Expected Results:**
- Counter increments once per 4096 AON ticks with prescaler=4095
- Exactly 4096 ticks produce exactly 1 count increment

**Pass/Fail Criteria:** Prescaler=4095 produces rate of exactly 1 count per 4096 AON ticks.

---

#### TC_AON_059

**Test Name:** Watchdog Count Read During Active Counting - Volatile Register Behavior

**Description:** Verify that reads of WDOG_COUNT return the live counter value that increments asynchronously on the AON clock. Multiple consecutive reads should return different (increasing) values when the watchdog is actively counting.

**Prerequisites:**
- Reset applied, watchdog enabled with thresholds above any expected count value

**Test Steps:**
1. Set WDOG_BARK_THOLD=0xFFFFFFFF, WDOG_BITE_THOLD=0xFFFFFFFF
2. Write WDOG_COUNT=0x00000000, WDOG_CTRL=0x00000001 (enable=1); perform read-back
3. Read WDOG_COUNT; record value V1
4. Advance simulation by 10 AON ticks
5. Read WDOG_COUNT; record value V2
6. Advance simulation by 10 more AON ticks
7. Read WDOG_COUNT; record value V3
8. Verify V2 > V1 and V3 > V2 (counter advancing between reads)

**Expected Results:**
- Consecutive reads of WDOG_COUNT with AON ticks between them return different increasing values
- Counter is volatile; live state is always returned by read callbacks

**Pass/Fail Criteria:** WDOG_COUNT reads return incrementing live values when watchdog is enabled.

---

#### TC_AON_060

**Test Name:** WKUP_COUNT_HI Read During Active Counting - Volatile 64-bit Counter

**Description:** Verify that reads of WKUP_COUNT_HI return the current upper 32 bits of the live 64-bit counter, updating correctly when LO overflows into HI.

**Prerequisites:**
- Reset applied, wakeup timer enabled

**Test Steps:**
1. Set WKUP_THOLD to maximum; write WKUP_COUNT to HI=0, LO=0xFFFFFFFD
2. Enable timer: WKUP_CTRL=0x00000001 (prescaler=0); perform read-back
3. Read WKUP_COUNT_HI; verify=0x00000000
4. Read WKUP_COUNT_LO; verify value in range 0xFFFFFFFD to 0xFFFFFFFF
5. Advance simulation 5 ticks (should cause LO to wrap and HI to increment)
6. Read WKUP_COUNT_HI; verify=0x00000001 (LO has overflowed into HI)
7. Read WKUP_COUNT_LO; verify low values (1, 2, 3 range after overflow)

**Expected Results:**
- WKUP_COUNT_HI correctly increments when WKUP_COUNT_LO overflows
- The 64-bit counter extends correctly across both registers

**Pass/Fail Criteria:** 64-bit counter carry propagates from LO to HI correctly.

---

## 6. Coverage Summary

| Test Category | Test IDs | Count |
|---|---|---|
| Register Reset and Access Type | TC_AON_001 through TC_AON_009 | 9 |
| Wakeup Timer Functional | TC_AON_010 through TC_AON_016 | 7 |
| Watchdog Timer Functional | TC_AON_017 through TC_AON_023 | 7 |
| Interrupt Tests | TC_AON_024 through TC_AON_028 | 5 |
| Power Management | TC_AON_029 through TC_AON_031 | 3 |
| Security Tests | TC_AON_032 through TC_AON_036 | 5 |
| 64-bit Non-Atomic Access | TC_AON_037 through TC_AON_040 | 4 |
| Reset Behavior | TC_AON_041 through TC_AON_043 | 3 |
| Escalation Tests | TC_AON_044 through TC_AON_047 | 4 |
| Edge and Corner Cases | TC_AON_048 through TC_AON_060 | 13 |
| **Total** | | **60** |

---

### Feature Coverage Matrix

| Feature | Test Case(s) |
|---|---|
| All 14 register reset values | TC_AON_001 |
| WKUP_CTRL RW access, reserved bits | TC_AON_002 |
| ALERT_TEST WO, no storage, fatal_fault pulse | TC_AON_003, TC_AON_034 |
| INTR_TEST WO, no storage | TC_AON_004 |
| INTR_STATE RW1C (W1C) semantics | TC_AON_005 |
| WKUP_CAUSE RW0C semantics | TC_AON_006 |
| WDOG_REGWEN write-once-clear lock | TC_AON_007, TC_AON_032, TC_AON_033 |
| Reserved bits read-as-zero | TC_AON_008 |
| Async write CDC read-back guarantee | TC_AON_009 |
| Wakeup timer enable/disable | TC_AON_010 |
| Prescaler operation (multiple values) | TC_AON_011 |
| Prescaler reset side-effect on WKUP_CTRL write | TC_AON_012, TC_AON_056 |
| Wakeup timer threshold comparison and interrupt | TC_AON_013 |
| Interrupt continuous re-triggering | TC_AON_014, TC_AON_054 |
| 64-bit threshold comparison correctness | TC_AON_015 |
| Counter software write and initialization | TC_AON_016 |
| Watchdog timer enable/disable | TC_AON_017 |
| Watchdog bark threshold and interrupt | TC_AON_018 |
| Watchdog bite threshold and aon_timer_rst_req | TC_AON_019, TC_AON_031 |
| Watchdog petting (any write resets to 0) | TC_AON_020, TC_AON_021 |
| Watchdog sleep pause feature | TC_AON_022 |
| Wakeup timer unaffected by sleep_mode | TC_AON_023 |
| INTR_TEST force-assert wakeup interrupt | TC_AON_024 |
| INTR_TEST force-assert bark and NMI coupling | TC_AON_025 |
| Interrupt deassertion below threshold | TC_AON_026, TC_AON_027 |
| INTR_STATE and WKUP_CAUSE independent clearing | TC_AON_028, TC_AON_057 |
| wkup_req persistence until WKUP_CAUSE clear | TC_AON_029 |
| wkup_req dual source (wakeup timer + watchdog bark) | TC_AON_030 |
| WDOG_REGWEN lock protection of three registers | TC_AON_032 |
| WDOG_COUNT petting always allowed (not locked) | TC_AON_033 |
| RACL access control (EnableRacl=1) | TC_AON_035 |
| RACL absent (EnableRacl=0) | TC_AON_036 |
| 64-bit safe counter read (double-read technique) | TC_AON_037 |
| 64-bit safe counter write (disable then write) | TC_AON_038 |
| 64-bit safe threshold write (spurious prevention) | TC_AON_039 |
| 64-bit threshold sequential read is race-free | TC_AON_040 |
| System reset restores all state | TC_AON_041 |
| Watchdog bite induced system reset | TC_AON_042 |
| rst_aon_n independent AON domain reset | TC_AON_043 |
| Escalation halts both counters immediately | TC_AON_044 |
| Escalation preserves existing interrupt state | TC_AON_045 |
| Escalation prevents watchdog bite | TC_AON_046 |
| Register access unaffected by escalation | TC_AON_047 |
| Counter above threshold on enable - immediate interrupt | TC_AON_048 |
| Equal bark and bite thresholds - simultaneous trigger | TC_AON_049 |
| Bite threshold lower than bark - bite before bark | TC_AON_050 |
| Zero-value bite threshold - immediate bite on enable | TC_AON_051 |
| 64-bit wakeup counter overflow wrap-around | TC_AON_052 |
| 32-bit watchdog counter overflow wrap-around | TC_AON_053 |
| Maximum prescaler (prescaler=4095) | TC_AON_058 |
| Volatile WDOG_COUNT reads during counting | TC_AON_059 |
| Volatile 64-bit counter HI carry propagation | TC_AON_060 |
| Both timers concurrent independent operation | TC_AON_055 |

---

### Interrupt Source Coverage

| Interrupt Source | Interrupt Output | Covered By |
|---|---|---|
| Wakeup timer threshold crossing | `intr_wkup_timer_expired` | TC_AON_013, TC_AON_024 |
| Watchdog bark threshold crossing | `intr_wdog_timer_bark` | TC_AON_018, TC_AON_025 |
| Watchdog bark NMI copy | `nmi_wdog_timer_bark` | TC_AON_018, TC_AON_025 |

### Power Management Output Coverage

| Power Management Output | Trigger Condition | Covered By |
|---|---|---|
| `wkup_req` (from wakeup timer) | WKUP_COUNT >= WKUP_THOLD | TC_AON_013, TC_AON_029 |
| `wkup_req` (from watchdog bark) | WDOG_COUNT >= WDOG_BARK_THOLD | TC_AON_030 |
| `aon_timer_rst_req` | WDOG_COUNT >= WDOG_BITE_THOLD | TC_AON_019, TC_AON_031, TC_AON_042 |

---

*End of AON Timer Test Plan*
