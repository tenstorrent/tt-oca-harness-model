# AON Timer Model High-Level Design Document

## TABLE OF CONTENTS

1. [Introduction](#1-introduction)
   - [1.1 Objective](#11-objective)
   - [1.2 Scope](#12-scope)
   - [1.3 Acronyms](#13-acronyms)
   - [1.4 Is list](#14-is-list)
   - [1.5 Is not list](#15-is-not-list)

2. [Functional Description](#2-functional-description)
   - [2.1 AON Timer Config. Parameters](#21-aon-timer-config-parameters)
   - [2.2 Port Interfaces](#22-port-interfaces)
   - [2.3 Memory-Mapped Registers](#23-memory-mapped-registers)

3. [Use Model](#3-use-model)
   - [3.1 Callbacks on Memory-Mapped Registers/Bit-Fields](#31-callbacks-on-memory-mapped-registersbit-fields)

4. [Assumptions](#4-assumptions)

---

## 1. Introduction

### 1.1 Objective

This document provides the design specifications for the Always-On Timer (AON Timer) as a SystemC TLM2 compliant model. The AON Timer will be modeled at the Loosely Timed (LT) abstraction level with timing annotation and temporal decoupling. The intended use case of this model is software development. In this design, behavior, communication, and timing are separated as far as possible. The functional description of the AON Timer along with information about its internal registers and interface ports are discussed in detail.

The AON Timer is a dual-timer peripheral comprising one 64-bit upcounting wakeup timer (WKUP) and one 32-bit upcounting watchdog timer (WDOG). It operates continuously in the always-on (AON) power domain and provides power management wakeup requests, watchdog bark interrupts, watchdog bite reset requests, and lifecycle escalation response to the surrounding SoC subsystem.

### 1.2 Scope

The scope of this document is to describe the design details of the AON Timer SystemC TLM model. It focuses on the implementation-level details that form the basis for developing the AON Timer LT model, including functional behavior that must be modeled to support software development. Interface details required for communicating with the outside world are also described.

The model covers all software-visible functional behavior: timer counting, threshold comparison, interrupt generation and acknowledgment, wakeup request management, watchdog petting and locking, power management integration, lifecycle escalation response, and optional RACL access control. Implementation-level details such as RTL synchronizer stages, physical clock modeling, DFT infrastructure, and cycle-accurate bus protocol internals are explicitly excluded.

### 1.3 Acronyms

| Acronym | Expansion |
| --- | --- |
| **AON** | Always-On |
| **CDC** | Clock Domain Crossing |
| **CSR** | Control and Status Register |
| **DFT** | Design for Testability |
| **IP** | Intellectual Property |
| **LT** | Loosely Timed |
| **NMI** | Non-Maskable Interrupt |
| **PLIC** | Platform-Level Interrupt Controller |
| **RACL** | Register Access Control Logic |
| **RTL** | Register Transfer Level |
| **SYS** | System Clock Domain |
| **TL-UL** | Tile Link - Uncached Lightweight |
| **TLM** | Transaction-Level Modeling |
| **WDOG** | Watchdog Timer |
| **WKUP** | Wakeup Timer |
| **W1C** | Write-1-to-Clear |
| **RW0C** | Read/Write-0-to-Clear |

### 1.4 Is list

The following features must be modeled in the AON Timer TLM implementation as they represent software-visible functional behavior.

#### Core Timer Functionality

**Wakeup Timer (WKUP) - 64-bit Upcounting Timer**

- 64-bit upcounting timer that starts counting from the value written to `WKUP_COUNT` registers
- Timer increments on each AON clock tick, with the effective increment rate controlled by the 12-bit prescaler: one count per `N + 1` AON clock cycles where `N` is the prescaler value
- Timer enable and disable via software through the `WKUP_CTRL.enable` bit
- Threshold comparison: wakeup event fires when count reaches or exceeds `WKUP_THOLD` (both HI and LO combined as a 64-bit value)
- Continuous re-triggering: if count remains at or above threshold after interrupt is cleared and timer remains enabled, the interrupt and wakeup signal re-assert at the next clock tick
- Software-writeable counter value for counter initialization and reset
- Prescaler reset occurs on every write to `WKUP_CTRL` (including writes that do not change the value); this side-effect is observable through the effective counting rate exposed to software

**Watchdog Timer (WDOG) - 32-bit Upcounting Timer**

- 32-bit upcounting timer with independent count from `WDOG_COUNT`
- Timer increments on each AON clock tick independently from the wakeup timer
- Timer enable and disable via software through the `WDOG_CTRL.enable` bit
- Dual-threshold architecture: bark threshold (`WDOG_BARK_THOLD`) and bite threshold (`WDOG_BITE_THOLD`) are independently programmable
- Watchdog petting mechanism: software resets the count to zero by writing any value to `WDOG_COUNT` (written data is discarded; any write has identical effect)

#### Interrupt Generation and Handling

**Wakeup Timer Interrupt**

- Level interrupt (`intr_wkup_timer_expired`) asserted in the SYS domain when the wakeup timer count reaches or exceeds `WKUP_THOLD`
- Interrupt remains asserted (level-sensitive) until software explicitly clears it by writing 1 to `INTR_STATE.wkup_timer_expired` (write-1-to-clear semantics)
- Interrupt status is software-readable via `INTR_STATE`
- Software testing via `INTR_TEST` register to force-assert the interrupt signal without a real timer event

**Watchdog Bark Interrupt**

- Level interrupt (`intr_wdog_timer_bark`) asserted in the SYS domain when the watchdog count reaches or exceeds `WDOG_BARK_THOLD`
- Interrupt remains asserted until software clears it by writing 1 to `INTR_STATE.wdog_timer_bark`
- NMI output (`nmi_wdog_timer_bark`) is a logical copy of the watchdog bark interrupt output driven identically and simultaneously, providing an alternative path to an NMI pin if required
- Software testing via `INTR_TEST` register

#### Power Management Integration

**Wakeup Request Signals**

- Level wakeup signal (`wkup_req`) asserted in the AON domain when either the wakeup timer threshold is reached or the watchdog bark threshold is reached
- `wkup_req` persists until system reset or software writes 0 to `WKUP_CAUSE` (write-0-to-clear semantics)
- `WKUP_CAUSE` register provides software-readable indication that a wakeup event has occurred; it is a single combined bit that does not discriminate between the wakeup timer and the watchdog bark as the source
- Software-controlled wakeup acknowledgment: writing 0 to `WKUP_CAUSE` de-asserts the wakeup request to the power manager

**Sleep Mode Interaction**

- Watchdog timer pause-in-sleep feature: when `WDOG_CTRL.pause_in_sleep` is set to 1 and the `sleep_mode` input is asserted, the watchdog counter halts
- This prevents premature watchdog expiration during long sleep periods managed by the wakeup timer
- Sleep mode pausing applies only to the watchdog timer; the wakeup timer is unaffected by `sleep_mode` and always continues counting (always-on behavior)

#### Reset and Escalation Handling

**Watchdog Bite Reset**

- Reset request (`aon_timer_rst_req`) asserted in the AON domain when the watchdog count reaches or exceeds `WDOG_BITE_THOLD` and the watchdog is enabled
- Reset request is independent of the bark interrupt path; both bark and bite conditions can be simultaneously active on separate output signals
- System reset triggered by the power manager clears all timer state automatically; no software acknowledgment is required after a watchdog bite reset

**Lifecycle Escalation Response**

- Both timers (WKUP and WDOG) halt counting immediately when `lc_escalate_en` is asserted, regardless of their enable bits (highest-priority override)
- This prevents either timer from interfering with system security escalation flows
- Halting behavior is a software-visible functional outcome: counter values do not advance while this signal is held asserted

#### Configuration and Control

**Watchdog Configuration Lock**

- Write-once-clear mechanism: writing 0 to `WDOG_REGWEN.regwen` permanently locks the watchdog configuration until the next system reset
- After lock, writes to `WDOG_CTRL`, `WDOG_BARK_THOLD`, and `WDOG_BITE_THOLD` are silently ignored; bus transactions complete normally with no error response
- `WDOG_COUNT` is explicitly NOT gated by this lock; watchdog petting is always permitted regardless of lock state
- `WDOG_REGWEN` resets to 1 (unlocked); writing 1 to it has no effect

**64-bit Counter Non-Atomic Access Behavior**

- The 64-bit wakeup counter and threshold are accessed as two separate 32-bit registers (HI and LO)
- Non-atomic access: the counter may increment between the HI and LO register reads, producing an incorrect combined 64-bit value if a LO overflow occurs between reads
- Software must implement multi-read sequences (read HI, read LO, re-read HI; if HI changed, re-read LO) to safely assemble the 64-bit value
- Timer must be disabled before writing both HI and LO counter registers to avoid race conditions; threshold writing must follow the safe three-step sequence (set LO to maximum, write new HI, write new LO) to prevent spurious interrupts

**Asynchronous Register Write Completion**

- Register writes from the SYS clock domain to the AON clock domain may complete at the bus interface before the data reaches the underlying AON-domain register
- Software can guarantee write completion by performing a read-back of the written register; the read stalls until all prior writes have propagated

#### Error and Alert Generation

**TL-UL Bus Integrity Alert**

- Fatal alert (`fatal_fault`) is generated when a fatal TL-UL bus integrity fault is detected
- `ALERT_TEST` register allows software to trigger a test alert to verify alert connectivity

**RACL Access Control (when EnableRacl parameter is set)**

- Register-level access control enforcement via incoming `racl_policies` policy vector
- RACL policy violations are reported via the `racl_error` inter-module signal
- Software-observable access violation detection

### 1.5 Is not list

The following are explicitly excluded from the AON Timer TLM model as they are implementation-level details with no software-visible functional impact.

Pre-emption shall not be supported in the AON Timer Model due to the blocking transport nature of TLM LT models. As the model is not timing accurate, it is not suitable for performance measurements.

#### Electrical and Physical Characteristics

**Clock Specifications**

- Precise ~200 kHz AON clock frequency value (the functional tick rate is modeled abstractly via timing annotations)
- Clock jitter, phase noise, and duty-cycle characteristics of the AON or SYS clock domains
- Clock domain crossing metastability resolution time and exact synchronizer propagation delays
- Setup and hold time constraints for CDC logic cells

**Voltage and Power Characteristics**

- AON domain supply voltage levels and supply sequencing
- Active and sleep mode power consumption values
- Leakage current specifications
- Temperature-dependent timing or frequency variations

#### Implementation-Level Timing Details

**CDC Synchronization Implementation**

- Exact number of synchronizer flip-flop stages in SYS-to-AON or AON-to-SYS paths
- Metastability resolution circuit topology
- Precise bit-level CDC propagation latency in clock cycles
- Clock edge alignment requirements between SYS and AON domains

**Pin-Level and Pad Timing**

- Signal propagation delays through I/O pads
- Rise and fall time specifications for output signals
- Input receiver switching thresholds
- Output driver strength settings

#### RTL Implementation Details

**Register Implementation**

- Specific flop types used (e.g., scan-enabled, reset-synchronous)
- Reset distribution tree structure and buffering
- Clock-gating cell placement for register enables
- Physical placement of registers in layout

**Counter Architecture**

- Ripple-carry versus look-ahead adder implementation for counter increment logic
- Threshold comparator circuit structure
- Internal split implementation details of the 64-bit wakeup counter in RTL (e.g., how carry propagates between the LO and HI 32-bit sub-counters at the gate level)

#### Test and Debug Infrastructure

**DFT Features**

- Scan chain insertion and scan enable signal handling
- ATPG test patterns and fault models
- Boundary scan (JTAG) implementation and test data registers
- Manufacturing test modes not exercised during normal software operation

**RTL Verification Artifacts**

- UVM testbench scoreboard logic and back-door register access mechanisms
- Functional coverage model implementation
- Assertion monitor signals internal to RTL or bound-in checkers
- TL-UL protocol assertion checker binding

#### Unimplemented and Reserved Functionality

- Bit fields marked as reserved in the register specification (e.g., bits 31:1 of `WDOG_REGWEN`, bits 31:13 of `WKUP_CTRL`)
- Synthesis options and timing constraint files
- Physical design parameters (floorplan, placement, routing)
- Parameters controlling internal pipeline depth or buffer sizes invisible to the register interface

#### Bus Protocol Implementation Details

- Cycle-by-cycle TL-UL handshaking (ready/valid) waveforms
- Bus arbitration priority and transaction ordering logic
- Protocol-specific error injection beyond the functional `fatal_fault` alert
- Exact number of bus cycles consumed by a register read or write

---

## 2. Functional Description

The AON Timer will be modeled at the LT abstraction level with timing annotation and temporal decoupling. The intended use case of this model is software development. The AON Timer peripheral provides two independent timers sharing the same AON clock domain: a 64-bit wakeup timer (WKUP) for generating long-duration wakeup events and a 32-bit watchdog timer (WDOG) for system liveness monitoring. Both timers are upcounting and generate outputs when their respective counters reach or exceed programmable thresholds.

### 2.1 AON Timer Config. Parameters

Configuration parameters determine the structural configuration of the AON Timer peripheral at instantiation time and cannot be changed at runtime.

#### Build-Time Configuration Parameters

| Parameter Name | Type | Default | Description |
| --- | --- | --- | --- |
| EnableRacl | Boolean | 0 (disabled) | Controls presence of the RACL (Register Access Control Logic) port pair. When set to 1, adds the `racl_policies` input port (incoming policy vector from a `racl_ctrl` instance) and `racl_error` output port (error log output) to the module interface. Enables per-register access control policy enforcement and RACL error reporting. When set to 0, these ports are omitted and no RACL enforcement is applied beyond standard TL-UL protocol behavior. Directly affects module interface structure and security behavior. |

#### Fixed Architectural Constants

The following attributes are fixed across all AON Timer variants and do not vary between instantiations. They are not build-time configuration parameters as no evidence of configurable variants exists.

| Attribute | Value | Description |
| --- | --- | --- |
| WKUP Counter Width | 64 bits | Wakeup timer counter width, accessed as two separate 32-bit registers: `WKUP_COUNT_HI` (bits 63:32) and `WKUP_COUNT_LO` (bits 31:0) |
| WDOG Counter Width | 32 bits | Watchdog timer counter width, accessed as the single 32-bit `WDOG_COUNT` register |
| WKUP Prescaler Width | 12 bits | Wakeup timer prescaler field width in `WKUP_CTRL.prescaler`; enables very long timeouts by slowing the count rate |
| Number of Timers | 2 | Fixed dual-timer architecture: one 64-bit wakeup timer and one 32-bit watchdog timer |
| WDOG Thresholds | 2 | Watchdog has exactly two independently programmable thresholds: bark (`WDOG_BARK_THOLD`, generates interrupt) and bite (`WDOG_BITE_THOLD`, generates reset request) |

#### Modeling Impact of EnableRacl Parameter

When `EnableRacl = 1`, the TLM model must add the `racl_policies` input port and `racl_error` output port to the module interface, implement per-register access control policy enforcement using the incoming policy vector before allowing read/write operations on each CSR, generate RACL error log information on access violations, and drive it via `racl_error`.

When `EnableRacl = 0`, the TLM model omits `racl_policies` and `racl_error` from the interface and allows unrestricted register access governed only by standard TL-UL protocol behavior.

#### Fixed Constants Modeling Impact

The fixed counter widths and prescaler width determine the following modeling constraints:

- Internal state storage: 64-bit wakeup counter state (stored as two 32-bit halves), 32-bit watchdog counter state, 12-bit prescaler counter state
- Register interface: dual 32-bit register access for the 64-bit wakeup counter and threshold; non-atomic access semantics must be modeled
- Threshold comparison logic: 64-bit comparison for wakeup timer expiry, 32-bit comparison for watchdog bark and bite
- Timeout range: approximately 6 hours maximum for the watchdog (32-bit at ~200 kHz), approximately 3 million years maximum for the wakeup timer (64-bit at ~200 kHz with prescaler)

### 2.2 Port Interfaces

The AON Timer TLM model interface abstracts hardware signals into TLM constructs while preserving all software-visible functional behavior.

#### Register Bus Interface

| Interface Category | Port Name | Port Type | Description |
| --- | --- | --- | --- |
| Register Bus | tl_socket | tlm_target_socket\<32\> | TL-UL register access interface (device/responder role). Provides access to all configuration, threshold, counter, interrupt, and status registers. The primary clock domain for bus transactions is the SYS domain. Register writes may complete at the bus interface before the written data reaches the underlying AON-domain register; software must perform a read-back to guarantee write completion. Named `tl` in the hardware interface specification. |

#### Interrupt Outputs (SYS Domain)

| Interface Category | Port Name | Port Type | Description |
| --- | --- | --- | --- |
| Interrupt | intr_wkup_timer_expired | sc_out\<bool\> | Level-sensitive wakeup timer interrupt driven in the SYS clock domain. Asserted when the 64-bit wakeup counter value reaches or exceeds `WKUP_THOLD` and `WKUP_CTRL.enable=1`. Remains asserted until software writes 1 to `INTR_STATE.wkup_timer_expired`. Re-asserts at the next simulated AON tick if the counter still meets or exceeds the threshold and the timer remains enabled. Testable via `INTR_TEST` register. |
| Interrupt | intr_wdog_timer_bark | sc_out\<bool\> | Level-sensitive watchdog bark interrupt driven in the SYS clock domain. Asserted when the 32-bit watchdog counter value reaches or exceeds `WDOG_BARK_THOLD` and `WDOG_CTRL.enable=1`. Remains asserted until software writes 1 to `INTR_STATE.wdog_timer_bark`. Testable via `INTR_TEST` register. |
| Interrupt | nmi_wdog_timer_bark | sc_out\<bool\> | Non-maskable interrupt output for watchdog bark events driven in the SYS clock domain. A logical copy of `intr_wdog_timer_bark`; driven identically and simultaneously. Provides an alternative path to an NMI pin when required by the system integration. |

#### Power Management Outputs (AON Domain)

| Interface Category | Port Name | Port Type | Description |
| --- | --- | --- | --- |
| Power Management | wkup_req | sc_out\<bool\> | Level wakeup request to the power manager driven in the AON clock domain. Asserted when either the wakeup timer threshold is reached or the watchdog bark threshold is reached. Remains asserted until a system reset occurs or software explicitly writes 0 to the `WKUP_CAUSE` register. |
| Power Management | aon_timer_rst_req | sc_out\<bool\> | Reset request to the power manager driven in the AON clock domain. Asserted when the 32-bit watchdog counter reaches or exceeds `WDOG_BITE_THOLD` and `WDOG_CTRL.enable=1`. Triggers a system reset independent of the bark interrupt path. The system reset from the power manager also resets the AON Timer itself; no software acknowledgment is required after a watchdog bite reset. |

#### Power Management Input (SYS Domain)

| Interface Category | Port Name | Port Type | Description |
| --- | --- | --- | --- |
| Power Management | sleep_mode | sc_in\<bool\> | Sleep mode indication received from the power manager. When asserted and `WDOG_CTRL.pause_in_sleep=1`, the watchdog counter halts counting. This prevents premature watchdog bark or bite during long sleep periods managed by the wakeup timer. The wakeup timer is unaffected by this signal and continues counting regardless. Named `sleep_mode_i` in the hardware specification. |

#### Lifecycle and Security Input

| Interface Category | Port Name | Port Type | Description |
| --- | --- | --- | --- |
| Lifecycle | lc_escalate_en | sc_in\<bool\> | Lifecycle escalation enable received from the lifecycle controller. RTL type is `lc_ctrl_pkg::lc_tx`; modeled as `sc_in<bool>` at TLM abstraction. When asserted, both the wakeup timer and the watchdog timer immediately halt counting, preventing either timer from interfering with system security escalation flows. Named `lc_escalate_en_i` in the hardware specification. |

#### Security Alert Output

| Interface Category | Port Name | Port Type | Description |
| --- | --- | --- | --- |
| Alert | fatal_fault | sc_out\<bool\> | Fatal alert output for TL-UL bus integrity violations. Asserted when a fatal TL-UL bus integrity fault is detected by the end-to-end integrity scheme. Software can trigger a test alert via the `ALERT_TEST.fatal_fault` write-only field to verify alert connectivity. |

#### Clock Inputs (Abstract Timing)

| Interface Category | Port Name | Port Type | Description |
| --- | --- | --- | --- |
| Clock | clk_aon_freq | sc_in\<double\> | AON clock frequency input in Hz (abstract, not a pin-level clock). Models functional timing for counter increment events, approximately 200 kHz typical. Counter increment events for both the wakeup timer and watchdog timer are scheduled at a rate derived from this frequency scaled by the prescaler `(N + 1)` for the wakeup timer and directly at this frequency for the watchdog timer. Temporal decoupling (quantum keeper) is used; no cycle-accurate modeling is performed. Replaces hardware pin `clk_aon_i`. |
| Clock | clk_sys_freq | sc_in\<double\> | System clock frequency input in Hz (abstract, not a pin-level clock). Models the TL-UL register interface timing domain (SYS domain). CDC effects from SYS to AON domain are modeled functionally as delays without cycle accuracy; no synchronizer flip-flop stages are modeled. Replaces hardware pin `clk_i`. |

#### Reset Inputs

| Interface Category | Port Name | Port Type | Description |
| --- | --- | --- | --- |
| Reset | rst_n | sc_in\<bool\> | Active-low system reset (SYS domain reset). Resets all timer state, clears both the wakeup and watchdog counters, de-asserts all interrupt and power management outputs, and restores `WDOG_REGWEN` to its unlocked default (reset value 0x1). |
| Reset | rst_aon_n | sc_in\<bool\> | Active-low AON domain reset. Resets AON-domain state and output registers including `wkup_req` and `aon_timer_rst_req`. In normal operation both resets are asserted and de-asserted together; modeled as separate ports to reflect the dual clock domain architecture and allow independent reset scenarios. |

#### Optional RACL Security Ports (Conditional on EnableRacl Parameter)

| Interface Category | Port Name | Port Type | Description |
| --- | --- | --- | --- |
| RACL Security | racl_policies | sc_in\<racl_policy_vec_t\> | Incoming RACL policy vector from a `racl_ctrl` instance. Present only when the build-time parameter `EnableRacl=1`. RTL type is `top_racl_pkg::racl_policy_vec`. The policy selection vector determines which access control policy applies to each register, enforcing per-register read/write permissions before allowing TL-UL access to CSRs. Named `racl_policies_i` in the hardware specification. |
| RACL Security | racl_error | sc_out\<racl_error_log_t\> | RACL error log output. Present only when the build-time parameter `EnableRacl=1`. RTL type is `top_racl_pkg::racl_error_log`. Reports access violation information when an unauthorized register access is attempted. Named `racl_error_o` in the hardware specification. |

#### Port Usage Notes

**Clock Domain Assignment**

The AON Timer operates across three distinct domains at the RTL level. The TLM model abstracts these as functional behaviors driven by two abstract clock frequency inputs:

- SYS Domain outputs: `intr_wkup_timer_expired`, `intr_wdog_timer_bark`, `nmi_wdog_timer_bark`, and the register interface via `tl_socket` operate on SYS timing (modeled with `clk_sys_freq`)
- AON Domain outputs: `wkup_req` and `aon_timer_rst_req` operate on AON timing (modeled with `clk_aon_freq`)
- Async (CDC) Domain: Register writes from `tl_socket` propagate functionally with a modeled CDC delay from SYS to AON domain using temporal decoupling (quantum keeper) without cycle accuracy

**Watchdog Bark vs. Bite Separation**

The watchdog timer produces two independent functional outputs:

- Bark (`WDOG_BARK_THOLD`): Drives `intr_wdog_timer_bark`, `nmi_wdog_timer_bark`, and contributes to `wkup_req`; cleared by software writing to `INTR_STATE` and `WKUP_CAUSE`
- Bite (`WDOG_BITE_THOLD`): Drives `aon_timer_rst_req` only; triggers a system reset via the power manager and requires no software acknowledgment (the reset itself clears all state)

### 2.3 Memory-Mapped Registers

The AON Timer module's registers are accessed through the peripheral bus at the module's base address (base address is integration-defined). All configuration, status, and data transfer is handled via these registers. All registers use the asynchronous register generation feature for clock domain crossing from the SYS clock domain to the AON clock domain.

#### Register Address Map

| Register Name | Offset | Size (bits) | Access | Reset Value | Description |
| --- | --- | --- | --- | --- | --- |
| **Alert and Test Registers** | | | | | |
| ALERT_TEST | 0x00 | 32 | WO | 0x00000000 | Alert test register. Bit[0]: `fatal_fault` - write 1 to trigger one fatal alert event. Reserved bits[31:1] read as zero, writes ignored. |
| **Wakeup Timer Registers** | | | | | |
| WKUP_CTRL | 0x04 | 32 | RW | 0x00000000 | Wakeup timer control. Bit[0]: `enable` (RW, reset 0x0) - set to 1 to start wakeup timer counting. Bits[12:1]: `prescaler` (RW, reset 0x0) - 12-bit pre-scaler value; counter increments once every (prescaler + 1) AON clock ticks. Reserved bits[31:13] read as zero, writes ignored. Side-effect: every write to this register resets the internal prescaler count to zero, even if the written value is unchanged. Async write (SYS-to-AON CDC); read-back required to guarantee write completion. |
| WKUP_THOLD_HI | 0x08 | 32 | RW | 0x00000000 | Wakeup timer threshold, upper 32 bits (bits[63:32] of 64-bit threshold). Field `threshold_hi`[31:0]. Non-atomic access: hardware does not modify threshold registers, so sequential reads are safe. Write safe sequence required for threshold updates to avoid spurious wakeup events. Async write; read-back guarantees completion. |
| WKUP_THOLD_LO | 0x0C | 32 | RW | 0x00000000 | Wakeup timer threshold, lower 32 bits (bits[31:0] of 64-bit threshold). Field `threshold_lo`[31:0]. Non-atomic access: write LO to 0xFFFFFFFF before updating HI to avoid spurious wakeup from transient lower threshold. Async write; read-back guarantees completion. |
| WKUP_COUNT_HI | 0x10 | 32 | RW | 0x00000000 | Wakeup timer counter, upper 32 bits (bits[63:32] of 64-bit counter). Field `count_hi`[31:0]. Non-atomic access: counter may increment between HI and LO reads/writes. Disable timer before writing. Use multi-read sequence (read HI, LO, HI again; if HI changed, re-read LO) for safe reads. Async write; read-back guarantees completion. |
| WKUP_COUNT_LO | 0x14 | 32 | RW | 0x00000000 | Wakeup timer counter, lower 32 bits (bits[31:0] of 64-bit counter). Field `count_lo`[31:0]. Non-atomic access: see WKUP_COUNT_HI notes. Async write; read-back guarantees completion. |
| **Watchdog Timer Registers** | | | | | |
| WDOG_REGWEN | 0x18 | 32 | RW0C | 0x00000001 | Watchdog timer write-enable register. Bit[0]: `regwen` (RW0C, reset 0x1) - when 1 (default at reset), watchdog configuration registers are writable; writing 0 permanently locks WDOG_CTRL, WDOG_BARK_THOLD, and WDOG_BITE_THOLD until the next system reset. Once cleared to 0, cannot be re-set by software. Reserved bits[31:1] read as zero, writes ignored. |
| WDOG_CTRL | 0x1C | 32 | RW | 0x00000000 | Watchdog timer control. Gated by WDOG_REGWEN: writes silently ignored when WDOG_REGWEN.regwen = 0. Bit[0]: `enable` (RW, reset 0x0) - set to 1 to start watchdog timer counting. Bit[1]: `pause_in_sleep` (RW, reset 0x0) - when set to 1 and `sleep_mode` is asserted, watchdog counter halts. Reserved bits[31:2] read as zero, writes ignored. Async write; read-back guarantees completion. |
| WDOG_BARK_THOLD | 0x20 | 32 | RW | 0x00000000 | Watchdog bark threshold. Gated by WDOG_REGWEN: writes silently ignored when WDOG_REGWEN.regwen = 0. Field `threshold`[31:0]. Bark (interrupt and wakeup request) fires when WDOG_COUNT >= this threshold and watchdog is enabled. Async write; read-back guarantees completion. |
| WDOG_BITE_THOLD | 0x24 | 32 | RW | 0x00000000 | Watchdog bite threshold. Gated by WDOG_REGWEN: writes silently ignored when WDOG_REGWEN.regwen = 0. Field `threshold`[31:0]. Bite (system reset request via `aon_timer_rst_req`) fires when WDOG_COUNT >= this threshold and watchdog is enabled. Independent of bark interrupt path. Async write; read-back guarantees completion. |
| WDOG_COUNT | 0x28 | 32 | RW | 0x00000000 | Watchdog timer counter value. Field `count`[31:0]. Software writes any value to "pet" the watchdog (counter is reset to zero regardless of the written value), preventing bark/bite events. NOT gated by WDOG_REGWEN; petting is always permitted. Counter increments on each AON clock tick when WDOG_CTRL.enable = 1 and not halted by sleep or escalation. Async write; read-back guarantees completion. |
| **Interrupt Registers** | | | | | |
| INTR_STATE | 0x2C | 32 | RW1C | 0x00000000 | Interrupt state register. Bit[0]: `wkup_timer_expired` (RW1C, reset 0x0) - set by hardware when wakeup timer count reaches or exceeds WKUP_THOLD; write 1 to clear. Bit[1]: `wdog_timer_bark` (RW1C, reset 0x0) - set by hardware when watchdog count reaches or exceeds WDOG_BARK_THOLD; write 1 to clear. Reserved bits[31:2] read as zero. Level-sensitive: if count remains >= threshold after clearing and timer remains enabled, interrupt re-asserts at the next AON clock tick. |
| INTR_TEST | 0x30 | 32 | WO | 0x00000000 | Interrupt test register. Write-only; reads return 0x0 (no storage). Bit[0]: `wkup_timer_expired` - write 1 to force-assert the wakeup timer expired interrupt for software testing. Bit[1]: `wdog_timer_bark` - write 1 to force-assert the watchdog bark interrupt for software testing. Reserved bits[31:2] read as zero. |
| **Wakeup Management Registers** | | | | | |
| WKUP_CAUSE | 0x34 | 32 | RW0C | 0x00000000 | Wakeup request status. Bit[0]: `cause` (RW0C, reset 0x0) - set by hardware (in AON domain) when either timer reaches its threshold and asserts `wkup_req`; write 0 to clear and de-assert the `wkup_req` signal to the power manager. Writing 1 has no effect. Persists until system reset or software clears it. Reserved bits[31:1] read as zero. |

#### Register Access Type Legend

| Abbreviation | Meaning |
| --- | --- |
| RO | Read Only |
| RW | Read/Write |
| WO | Write Only |
| W1C | Write-1-to-Clear |
| RW1C | Read/Write-1-to-Clear |
| RW0C | Read/Write-0-to-Clear |

#### Register Notes

1. **Reserved Bits**: All reserved bits read as zero. Writes to reserved bits are ignored.
2. **Clock Domain**: All register accesses originate in the SYS clock domain and are synchronized to the AON clock domain via CDC logic. All registers exhibit asynchronous write completion behavior.
3. **64-bit Access**: WKUP_COUNT and WKUP_THOLD are 64-bit values accessed as two 32-bit register pairs (HI/LO). Non-atomic access requires the special handling sequences described in Section 3.
4. **Watchdog Locking**: Once WDOG_REGWEN.regwen is cleared to 0, watchdog configuration registers (WDOG_CTRL, WDOG_BARK_THOLD, WDOG_BITE_THOLD) are read-only until system reset. WDOG_COUNT remains writable at all times.
5. **Escalation Halt**: Both timers halt counting when `lc_escalate_en` is asserted, preventing timer interference during system security escalation.
6. **Sleep Pause**: Watchdog counting halts when `sleep_mode` is asserted AND WDOG_CTRL.pause_in_sleep = 1. The wakeup timer is unaffected by `sleep_mode`.
7. **Level Interrupts**: Both interrupt outputs are level-sensitive. They remain asserted until cleared via INTR_STATE and re-assert immediately on the next AON clock tick if the threshold condition persists.
8. **Wakeup Request Persistence**: `wkup_req` remains asserted until system reset or software explicitly writes 0 to WKUP_CAUSE.cause.
9. **NMI Output**: `nmi_wdog_timer_bark` is a direct logical copy of `intr_wdog_timer_bark`. Clearing INTR_STATE.wdog_timer_bark clears both outputs.
10. **RACL Access Control**: When `EnableRacl=1`, per-register access control is enforced via the `racl_policies` input. Policy violations are reported via `racl_error`. When `EnableRacl=0`, no RACL enforcement is applied.
11. **INTR_TEST No Storage**: INTR_TEST is write-only with no storage; reads always return 0x0.

---

## 3. Use Model

The AON Timer will be modeled at the LT abstraction level with timing annotation and temporal decoupling. The intended use case of this model is software development.

All counter increment events and functional delays are modeled using SystemC `sc_time`-based event scheduling driven by the abstract clock frequency parameters. Temporal decoupling via a quantum keeper is used for all timing without cycle-accurate synchronization. The TL-UL bus interface is abstracted to a `tlm_target_socket<32>` providing atomic register transactions; cycle-by-cycle bus protocol internals are excluded.

The key software-visible behaviors that drive the use model are:

- Software enables and configures the wakeup timer or watchdog timer via register writes, then polls or awaits interrupts to determine when thresholds are crossed
- Interrupt acknowledgment requires writing to `INTR_STATE` (W1C) and, independently, writing to `WKUP_CAUSE` (RW0C) if the wakeup request path must also be cleared
- Watchdog petting requires writing any value to `WDOG_COUNT` before the watchdog bark threshold is reached; the counter resets to zero regardless of the written value
- Software can lock the watchdog configuration permanently until reset by writing 0 to `WDOG_REGWEN.regwen`
- All register writes must be followed by a read-back to guarantee propagation to the AON clock domain

### 3.1 Callbacks on Memory-Mapped Registers/Bit-Fields

A register requires a callback when a register access triggers immediate hardware actions beyond simple storage updates.

**Write callback criteria:** Immediate functional side-effects (state change, counter modification, event clearing, triggering output signals), conditional write access (register gated by a lock bit), or prescaler/counter reset as a consequence of the write.

**Read callback criteria:** Volatile live state (counter values that increment asynchronously), interrupt and wakeup status reflecting live output signal state, or registers requiring read-back to confirm CDC write completion.

All 14 AON Timer registers require at least one callback. No register is storage-only.

#### Register Callbacks Table

| Callback Name | Type | Description |
| --- | --- | --- |
| handle_write_ALERT_TEST | Write | Triggers the `fatal_fault` alert output. Writing 1 to bit[0] immediately asserts the `fatal_fault` output signal to the alert handler for one test event. Bit[0] is write-only (reads return 0). Reserved bits[31:1] are ignored. |
| handle_write_WKUP_CTRL | Write | Controls wakeup timer enable and prescaler. Critical side-effect: every write to this register resets the internal prescaler counter to zero regardless of whether the written value differs from the current value. Must update the `enable` bit (start or stop wakeup counter increments), update the `prescaler` field value, reset the 12-bit prescaler accumulator to 0, and reschedule the next counter increment event. Async write; read-back required to guarantee write completion. |
| handle_read_WKUP_CTRL | Read | Returns current wakeup timer control register state. Read-back is the software mechanism to verify that an async SYS-to-AON write has completed and the written value has propagated to the AON domain register. |
| handle_write_WKUP_THOLD_HI | Write | Updates the upper 32 bits (bits[63:32]) of the 64-bit wakeup threshold. After storing the new value, must trigger an immediate threshold comparison: if the assembled 64-bit counter value >= the assembled 64-bit threshold and WKUP_CTRL.enable = 1, assert `intr_wkup_timer_expired` and `wkup_req`. Part of a non-atomic 64-bit register pair. |
| handle_read_WKUP_THOLD_HI | Read | Returns the upper 32 bits of the current wakeup threshold. Hardware does not modify threshold registers autonomously, so sequential reads are safe. Read-back required after write to confirm async write completion. |
| handle_write_WKUP_THOLD_LO | Write | Updates the lower 32 bits (bits[31:0]) of the 64-bit wakeup threshold. After storing the new value, must trigger an immediate threshold comparison: if the combined 64-bit counter >= combined 64-bit threshold and WKUP_CTRL.enable = 1, assert `intr_wkup_timer_expired` and `wkup_req`. Part of a non-atomic 64-bit register pair. |
| handle_read_WKUP_THOLD_LO | Read | Returns the lower 32 bits of the current wakeup threshold. Read-back required after write to confirm async write completion. |
| handle_write_WKUP_COUNT_HI | Write | Updates the upper 32 bits (bits[63:32]) of the 64-bit wakeup counter. After storing the new value, must trigger an immediate threshold comparison. Part of a non-atomic 64-bit register pair; timer should be disabled before writing to avoid race conditions. |
| handle_read_WKUP_COUNT_HI | Read | Returns the upper 32 bits of the live 64-bit wakeup counter. Value is volatile; the counter increments asynchronously on the AON clock. Read callback must return the current state, not a stale cached value. |
| handle_write_WKUP_COUNT_LO | Write | Updates the lower 32 bits (bits[31:0]) of the 64-bit wakeup counter. After storing the new value, must trigger an immediate threshold comparison. Part of a non-atomic 64-bit register pair. |
| handle_read_WKUP_COUNT_LO | Read | Returns the lower 32 bits of the live 64-bit wakeup counter. Value is volatile. Read callback must return current state, not a stale cached value. |
| handle_write_WDOG_REGWEN | Write | Implements RW0C semantics for the watchdog write-enable lock. Writing 0 to bit[0] clears `regwen` and permanently locks WDOG_CTRL, WDOG_BARK_THOLD, and WDOG_BITE_THOLD. Writing 1 has no effect. Once locked, the internal lock state is set and remains set until system reset. |
| handle_read_WDOG_REGWEN | Read | Returns current value of the write-enable register (1 = unlocked, 0 = locked). Read-back required to confirm async write completion. |
| handle_write_WDOG_CTRL | Write | Controls watchdog timer enable and pause-in-sleep. Must check the internal lock state: if locked, silently discard the write (bus transaction completes without error). If not locked, update the `enable` and `pause_in_sleep` fields and trigger an immediate threshold comparison. Async write; read-back required to guarantee write completion. |
| handle_read_WDOG_CTRL | Read | Returns current watchdog control register state. Read-back required after write to confirm async write completion. |
| handle_write_WDOG_BARK_THOLD | Write | Updates the watchdog bark threshold. Must check the internal lock state: if locked, silently discard the write. If not locked, store the new threshold value and trigger an immediate threshold comparison: if WDOG_COUNT >= new bark threshold and WDOG_CTRL.enable = 1, assert `intr_wdog_timer_bark`, `nmi_wdog_timer_bark`, and `wkup_req`. Async write; read-back required. |
| handle_read_WDOG_BARK_THOLD | Read | Returns the current watchdog bark threshold value. Read-back required after write to confirm async write completion. |
| handle_write_WDOG_BITE_THOLD | Write | Updates the watchdog bite threshold. Must check the internal lock state: if locked, silently discard the write. If not locked, store the new threshold value and trigger an immediate threshold comparison: if WDOG_COUNT >= new bite threshold and WDOG_CTRL.enable = 1, assert `aon_timer_rst_req`. Async write; read-back required. |
| handle_read_WDOG_BITE_THOLD | Read | Returns the current watchdog bite threshold value. Read-back required after write to confirm async write completion. |
| handle_write_WDOG_COUNT | Write | Primary watchdog petting mechanism. Any write to this register resets the watchdog counter to zero regardless of the written value (the data value is discarded). After forcing counter to 0, must trigger immediate threshold comparisons: since 0 < WDOG_BARK_THOLD (normally true), bark interrupt and `wkup_req` are de-asserted if previously active; since 0 < WDOG_BITE_THOLD (normally true), `aon_timer_rst_req` is de-asserted if previously active. NOT gated by WDOG_REGWEN; watchdog petting is always permitted regardless of lock state. |
| handle_read_WDOG_COUNT | Read | Returns the current live 32-bit watchdog counter value. Value is volatile: the counter increments asynchronously on each AON clock tick when WDOG_CTRL.enable = 1, subject to pause-in-sleep and lifecycle escalation. Read callback must return the current counter state, not a stale cached value. |
| handle_write_INTR_STATE | Write | Implements W1C (write-1-to-clear) interrupt acknowledgment. Bit[0]: `wkup_timer_expired`; Bit[1]: `wdog_timer_bark`. For each bit where the written value is 1, clear the corresponding INTR_STATE bit to 0 and de-assert the corresponding output signal. Writing 0 to a bit has no effect. After the W1C action, immediately re-evaluate threshold conditions: if the threshold condition still holds, the interrupt will re-assert on the next AON clock tick. |
| handle_read_INTR_STATE | Read | Returns the current interrupt pending status. Bit[0]: `wkup_timer_expired`; Bit[1]: `wdog_timer_bark`. Reflects the live interrupt assertion state (volatile). |
| handle_write_INTR_TEST | Write | Forces interrupt assertion for software testing. Writing 1 to bit[0] immediately sets INTR_STATE.wkup_timer_expired and asserts `intr_wkup_timer_expired`. Writing 1 to bit[1] immediately sets INTR_STATE.wdog_timer_bark and asserts `intr_wdog_timer_bark` and `nmi_wdog_timer_bark`. INTR_TEST is write-only; reads return 0x0 (no storage). |
| handle_write_WKUP_CAUSE | Write | Implements RW0C semantics for wakeup cause acknowledgment. Writing 0 to bit[0] clears `cause` and de-asserts `wkup_req`. Writing 1 has no effect. This clears the power manager wakeup request path independently of `INTR_STATE`. |
| handle_read_WKUP_CAUSE | Read | Returns current wakeup cause status. Bit[0]: set when `wkup_req` is asserted (by either timer threshold crossing). Read-back required after write to confirm async write completion. |

#### Output Signals Updated by Callbacks

| Output Signal | Updated By |
| --- | --- |
| `intr_wkup_timer_expired` | handle_write_INTR_STATE (clear), handle_write_INTR_TEST (force), threshold comparison after WKUP_CTRL / WKUP_THOLD_HI / WKUP_THOLD_LO / WKUP_COUNT_HI / WKUP_COUNT_LO writes |
| `intr_wdog_timer_bark` | handle_write_INTR_STATE (clear), handle_write_INTR_TEST (force), handle_write_WDOG_COUNT (pet), threshold comparison after WDOG_CTRL / WDOG_BARK_THOLD / WDOG_COUNT writes |
| `nmi_wdog_timer_bark` | Same triggers as `intr_wdog_timer_bark`; this signal is a wire copy |
| `wkup_req` | handle_write_WKUP_CAUSE (clear), threshold comparison after any wakeup or bark threshold crossing |
| `aon_timer_rst_req` | handle_write_WDOG_COUNT (clear via pet), threshold comparison after WDOG_BITE_THOLD / WDOG_COUNT / WDOG_CTRL writes |
| `fatal_fault` | handle_write_ALERT_TEST (assert on write-1 to bit[0]) |

#### Hardware State Variables Required in Callbacks

| State Variable | Width | Description |
| --- | --- | --- |
| `wkup_counter` | 64-bit | Live wakeup counter value; incremented by counter tick events |
| `wkup_threshold` | 64-bit | Assembled from `{WKUP_THOLD_HI, WKUP_THOLD_LO}` |
| `wkup_prescaler_count` | 12-bit | Prescaler accumulator; reset to 0 on every WKUP_CTRL write |
| `wkup_enabled` | bool | From `WKUP_CTRL.enable`; controls counter increment scheduling |
| `wdog_counter` | 32-bit | Live watchdog counter value; incremented by counter tick events |
| `wdog_bark_threshold` | 32-bit | From `WDOG_BARK_THOLD` |
| `wdog_bite_threshold` | 32-bit | From `WDOG_BITE_THOLD` |
| `wdog_enabled` | bool | From `WDOG_CTRL.enable` |
| `wdog_pause_in_sleep` | bool | From `WDOG_CTRL.pause_in_sleep` |
| `wdog_regwen_locked` | bool | Set when `WDOG_REGWEN.regwen` is written 0; permanent until reset |
| `intr_state_wkup` | bool | Current state of `INTR_STATE.wkup_timer_expired` |
| `intr_state_bark` | bool | Current state of `INTR_STATE.wdog_timer_bark` |
| `wkup_cause_active` | bool | Current state of `WKUP_CAUSE.cause` / `wkup_req` output |

#### Callback Implementation Notes

**WKUP_CTRL Prescaler Reset**

Every write to `WKUP_CTRL`, including writes that do not change any field value, resets the internal prescaler accumulator to zero. This side-effect is observable through the effective wakeup timer increment rate. The callback must unconditionally reset the prescaler count as part of the write handler before scheduling the next counter increment event, then reschedule the next increment event based on the new prescaler value.

**WDOG_COUNT Write Semantics**

Any write to `WDOG_COUNT` resets the counter to zero regardless of the written data value. Writing 0x00000000 and writing 0x12345678 have identical effects. The written value is discarded; the counter is unconditionally forced to 0, and immediate threshold comparisons are triggered.

**INTR_STATE W1C Semantics**

Writing 0 to a bit in `INTR_STATE` has no effect; only writing 1 clears the bit and de-asserts the corresponding output. After clearing, the threshold condition must be immediately re-evaluated: if it still holds (counter still >= threshold and timer still enabled), the interrupt will re-assert on the next AON clock tick.

**WKUP_CAUSE RW0C Semantics**

Writing 0 to bit[0] clears `cause` and de-asserts `wkup_req`. Writing 1 has no effect. `INTR_STATE` and `WKUP_CAUSE` serve independent purposes and require separate acknowledgment sequences: `INTR_STATE` clears the processor interrupt path; `WKUP_CAUSE` clears the power manager wakeup request path.

**WDOG_REGWEN Permanent Lock**

`WDOG_REGWEN` is a write-once-clear register (reset value 0x1). Once software writes 0, the `regwen` bit is cleared permanently and cannot be restored until the next system reset. Lock enforcement for `WDOG_CTRL`, `WDOG_BARK_THOLD`, and `WDOG_BITE_THOLD` is implemented in their respective write callbacks. Silent discard is the correct behavior when a locked register is written: the bus transaction completes with no error response.

**Threshold Comparison After Writes**

Write callbacks for all threshold registers and counter registers must trigger an immediate comparison after storing the new value. If counter >= threshold and timer enable = 1, the corresponding interrupt, wakeup request, or reset request is asserted. If counter < threshold, the corresponding output is de-asserted.

**Sleep Mode and Escalation Interaction**

`WDOG_COUNT` increments only when: WDOG_CTRL.enable = 1 AND NOT (WDOG_CTRL.pause_in_sleep = 1 AND `sleep_mode` input asserted) AND NOT `lc_escalate_en` asserted.

`WKUP_COUNT` increments only when: WKUP_CTRL.enable = 1 AND NOT `lc_escalate_en` asserted. The wakeup timer never pauses in sleep (always-on behavior).

`lc_escalate_en` halts both counters at highest priority, overriding all enable bits.

#### Callback Summary Statistics

| Metric | Count |
| --- | --- |
| Total write callbacks | 17 |
| Total read callbacks | 13 |
| Total unique registers with callbacks | 14 |
| Registers with both read and write callbacks | 12 |
| Registers with write callback only | 2 (ALERT_TEST, INTR_TEST - write-only, no read callback) |
| Registers excluded as storage-only | 0 |
| Registers with conditional write gating (WDOG_REGWEN) | 3 (WDOG_CTRL, WDOG_BARK_THOLD, WDOG_BITE_THOLD) |

---

## 4. Assumptions

The following assumptions govern the AON Timer SystemC TLM implementation and establish the abstraction level, timing behavior, and functional boundaries for virtual platform modeling.

### 1. Abstraction Level

1. **TLM-2.0 Loosely Timed (LT) abstraction** - The model conforms to TLM-2.0 LT abstraction, focusing on high-level functional behavior. Simulation speed takes priority over cycle accuracy; timing annotations are used for performance estimation only.

2. **Temporal decoupling via quantum keeper** - All functional delays (counter increments, CDC crossings, interrupt/reset assertion timing) are modeled using temporal decoupling with a quantum keeper. No cycle-accurate synchronizer stages, clock edge alignment, or precise propagation counts are modeled.

3. **No pin-level clock or electrical modeling** - Hardware clock pins (`clk_i`, `clk_aon_i`) are replaced by abstract scalar frequency inputs (`clk_sys_freq`, `clk_aon_freq`). Voltage levels, power supply sequencing, clock jitter, duty cycle, setup/hold times, and temperature-dependent variations are excluded entirely.

### 2. Clock Domain Handling

4. **Dual abstract clock domain scheduling** - AON domain (~200 kHz) and SYS domain clocks are modeled as frequency parameters driving `sc_time`-based event scheduling. Both the wakeup timer and watchdog timer increment on AON clock events. Interrupt outputs (`intr_wkup_timer_expired`, `intr_wdog_timer_bark`, `nmi_wdog_timer_bark`) are driven in the SYS domain. Power management outputs (`wkup_req`, `aon_timer_rst_req`) are driven in the AON domain.

5. **Wakeup timer prescaler modeled as counter enable gating** - The 12-bit prescaler in `WKUP_CTRL.prescaler` causes the wakeup counter to increment once every `(prescaler + 1)` AON clock ticks. Effective increment rate is `clk_aon_freq / (prescaler + 1)`. The watchdog timer increments directly at `clk_aon_freq` with no prescaler.

6. **WKUP_CTRL write unconditionally resets prescaler accumulator** - Every write to the `WKUP_CTRL` register, regardless of whether the written value differs from the current value, resets the internal prescaler count to zero. This side-effect is observable through the effective wakeup timer counting rate.

### 3. CDC Abstraction

7. **Asynchronous register write completion - functional model only** - All registers use the async register generation feature for SYS-to-AON CDC. TL-UL writes complete at the bus interface (returning a successful response) before the written data reaches the underlying AON-domain register. The CDC crossing latency is modeled as a functional quantum-keeper delay, not as a counted number of clock cycles. Software guarantees write completion by performing a read-back of the written register; the read stalls until all prior writes have propagated.

8. **CDC synchronizer implementation excluded** - The exact number of synchronizer flip-flop stages, metastability resolution circuits, and precise bit-level CDC propagation latency are not modeled. Only the software-visible functional consequence (write-before-effect, read-back stall) is preserved.

### 4. Counter Modeling

9. **64-bit wakeup counter - non-atomic HI/LO access** - The 64-bit wakeup counter and threshold are accessed as two independent 32-bit registers. The model allows the counter to advance between HI and LO register accesses, correctly modeling the software-visible race condition where a LO overflow between reads produces an incorrect 64-bit value. No hardware atomicity is provided; software must use the multi-read safe sequence.

10. **64-bit threshold write race - spurious wakeup risk modeled** - Writing `WKUP_THOLD` requires the safe write sequence (set LO to 0xFFFFFFFF, write new HI, write new LO) to prevent a transient low interim threshold from triggering a spurious wakeup interrupt. The model triggers an immediate threshold comparison after each threshold register write; software is responsible for using the safe sequence.

11. **32-bit watchdog counter - atomic access** - `WDOG_COUNT` is a single 32-bit register with atomic access. No HI/LO split and no non-atomic race condition applies to the watchdog counter or its thresholds.

12. **Counter comparison fires when count >= threshold** - Interrupt, wakeup request, and reset request are triggered when the counter value reaches or exceeds the respective threshold (greater-than-or-equal comparison), not only on exact match. This applies to all three comparison paths: wakeup expiry, watchdog bark, and watchdog bite.

13. **Counter overflow wraps to zero** - Both the 64-bit wakeup counter and the 32-bit watchdog counter use standard unsigned integer arithmetic with natural wrap-around at overflow. No saturation, overflow flag, or special handling beyond wrap to zero is modeled.

14. **WDOG_COUNT write is a watchdog pet - any write resets counter to zero** - Any write to `WDOG_COUNT`, regardless of the value written, resets the watchdog counter to zero. The write data value is discarded.

### 5. Interrupt Modeling

15. **Level-sensitive interrupts with W1C clear semantics** - Both `intr_wkup_timer_expired` and `intr_wdog_timer_bark` are level-sensitive outputs driven in the SYS domain. They assert when the threshold condition is met and the respective timer is enabled, and remain asserted until software writes 1 to the corresponding `INTR_STATE` bit. Writing 0 to an INTR_STATE bit has no effect.

16. **Continuous re-triggering on persistent threshold condition** - If the threshold condition persists after software clears `INTR_STATE` (counter still >= threshold and timer still enabled), the interrupt re-asserts at the next simulated AON clock tick. This applies to both wakeup and watchdog bark interrupts.

17. **NMI output is a wire copy of watchdog bark interrupt** - `nmi_wdog_timer_bark` is driven identically and simultaneously to `intr_wdog_timer_bark` at all times. No independent logic or separate clear mechanism exists; clearing `INTR_STATE.wdog_timer_bark` de-asserts both outputs.

18. **INTR_TEST forces interrupt assertion for software testing** - Writing 1 to `INTR_TEST` bit fields immediately sets the corresponding `INTR_STATE` bit and asserts the corresponding interrupt output, regardless of timer state or counter value. `INTR_TEST` is write-only; reads return 0x0 (no storage flip-flops).

### 6. Power Management

19. **Dual-source wakeup request with RW0C clear semantics** - `wkup_req` (AON domain) asserts when either the wakeup timer threshold is reached or the watchdog bark threshold is reached. It persists until a system reset or until software writes 0 to `WKUP_CAUSE.cause`. Writing 1 to `WKUP_CAUSE` has no effect. `WKUP_CAUSE` and `INTR_STATE` serve independent purposes and require separate acknowledgment sequences.

20. **WKUP_CAUSE is a single combined bit with no source discrimination** - `WKUP_CAUSE.cause` is a single-bit register set by hardware when `wkup_req` is asserted by either timer. It does not independently identify whether the wakeup timer or the watchdog bark was the triggering source. Software can infer the triggering timer by additionally reading `INTR_STATE`.

21. **Watchdog pause-in-sleep applies only to watchdog counter** - When `WDOG_CTRL.pause_in_sleep=1` and the `sleep_mode` input is asserted, the watchdog counter halts. The wakeup timer is unaffected by `sleep_mode` and always continues counting (always-on behavior).

### 7. Reset and Escalation

22. **System reset clears all timer state** - Asserting system reset (`rst_n` / `rst_aon_n`) clears both counters to zero, disables both timers (enable bits reset to 0), restores `WDOG_REGWEN` to its unlocked default (reset value 0x1), de-asserts all interrupt outputs, de-asserts `wkup_req` and `aon_timer_rst_req`, and clears `INTR_STATE` and `WKUP_CAUSE`. No software acknowledgment is required after a watchdog bite system reset; the reset from the power manager clears all timer state automatically.

23. **Lifecycle escalation halts both timers with highest priority** - When `lc_escalate_en` is asserted, both wakeup and watchdog counters immediately cease advancing regardless of their enable bits. This override has higher priority than any software-controlled enable state. Timers do not produce new threshold events while escalation is active.

24. **Watchdog bite reset request is independent of bark interrupt** - `aon_timer_rst_req` (AON domain) asserts when `WDOG_COUNT >= WDOG_BITE_THOLD` and `WDOG_CTRL.enable=1`. This path is completely independent of the bark interrupt (`intr_wdog_timer_bark`) and wakeup request (`wkup_req`) paths; both bite and bark conditions can be active simultaneously with separate outputs.

### 8. Security and Locking

25. **Watchdog configuration lock via WDOG_REGWEN (RW0C, write-once-clear)** - `WDOG_REGWEN.regwen` resets to 1 (unlocked). Writing 0 permanently locks `WDOG_CTRL`, `WDOG_BARK_THOLD`, and `WDOG_BITE_THOLD`; subsequent writes to these registers are silently ignored (bus transaction completes normally without error). Writing 1 to `WDOG_REGWEN` has no effect. The lock is irreversible until the next system reset. `WDOG_COUNT` is explicitly NOT gated by this lock and remains writable for watchdog petting at all times.

26. **RACL access control conditional on EnableRacl build-time parameter** - When `EnableRacl=1`, the model adds `racl_policies` input port and `racl_error` output port, enforces per-register access control from the incoming policy vector before allowing TL-UL CSR accesses, and reports access violations via `racl_error`. When `EnableRacl=0`, these ports are absent and no access control beyond standard TL-UL behavior is applied. The guide does not mention RACL (Silent Omission); the knowledge base explicitly documents this parameter and its ports. As this is a Silent Omission with no address collision or exhaustive exclusion, RACL is included.

27. **Fatal alert output on TL-UL bus integrity violation** - `fatal_fault` alert asserts when a fatal TL-UL bus integrity fault is detected. `ALERT_TEST.fatal_fault` write-only field triggers a software-initiated test alert event for connectivity verification. Detailed TL-UL protocol error injection, internal ECC correction logic, and recovery mechanisms are excluded.

### 9. Bus Interface

28. **TL-UL bus abstracted to TLM-2.0 target socket with atomic register transactions** - The TL-UL bus interface is modeled as a `tlm_target_socket<32>`. All register read and write accesses are atomic at the TLM transaction level. Cycle-by-cycle TL-UL handshaking (ready/valid waveforms), bus arbitration ordering, and the exact number of bus cycles per transaction are excluded.

29. **Fixed architectural constants - no software-visible parameterization** - 64-bit wakeup counter, 32-bit watchdog counter, 12-bit prescaler field, 2-timer architecture (one wakeup and one watchdog), and 2 watchdog thresholds (bark and bite) are immutable architectural attributes of this IP. No variant with different counter widths or timer counts exists in either source.
