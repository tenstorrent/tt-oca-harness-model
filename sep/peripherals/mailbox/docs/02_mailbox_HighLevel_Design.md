# Mailbox Model High-Level Design Document

## TABLE OF CONTENTS

1. [Introduction](#1-introduction)
   - [1.1 Objective](#11-objective)
   - [1.2 Scope](#12-scope)
   - [1.3 Acronyms](#13-acronyms)
   - [1.4 Is list](#14-is-list)
   - [1.5 Is not list](#15-is-not-list)

2. [Functional Description](#2-functional-description)
   - [2.1 Unit structure: channels behind one port](#21-unit-structure-channels-behind-one-port)
   - [2.2 Mailbox Config. Parameters](#22-mailbox-config-parameters)
   - [2.3 Port interfaces](#23-port-interfaces)
   - [2.4 Memory-Mapped Registers](#24-memory-mapped-registers)

3. [Use model](#3-use-model)
   - [3.1 Callbacks on Memory-mapped registers/bit-fields](#31-callbacks-on-memory-mapped-registersbit-fields)

4. [Assumptions](#4-assumptions)

## 1. Introduction

### 1.1 Objective

This document provides the design specifications for the Mailbox Unit as a SystemC TLM2 compliant model. This will be modeled at LT abstraction level with timing annotation and temporal decoupling. The intended use case of this model is software development. In this design we shall try to separate behavior, communication and timing as far as possible. The functional description of Mailbox Unit along with the information about the internal registers and interface ports are discussed in detail.

### 1.2 Scope

The scope of the document is to describe the design details of the Mailbox Unit. It mainly focuses on the implementation level details of the Mailbox Unit that in turn would form the basis for developing the Mailbox LT model. Also, the interface details are discussed which would be used for communicating with the outside world. The Mailbox Unit provides FIFO-based bidirectional communication channels for inter-processor or inter-chiplet communication using AXI4-Lite interfaces.

### 1.3 Acronyms

| **Acronym** | **Description** |
| --- | --- |
| LT | Loosely Timed |
| TLM | Transaction-Level Modeling |
| FIFO | First-In-First-Out |
| AXI | Advanced eXtensible Interface |
| RDL | Register Description Language |
| IRQ | Interrupt Request |
| EIRQ | Error Interrupt Request |
| RTIRQ | Read Threshold Interrupt Request |
| WTIRQ | Write Threshold Interrupt Request |

### 1.4 Is list

The following features are included in the TLM model:

#### Core Communication Features
- FIFO-Based Bidirectional Communication: Dual FIFO architecture connecting two AXI4-Lite slave ports where data written on port 0 appears on port 1 and vice versa
- Multiple Mailbox Instances: Support for multiple independent configurable mailbox pairs with dedicated control and status
- 64-bit Data Width: All registers and data transfers use a fixed 64-bit width (`regwidth=64`)
- Configurable FIFO Depth: Programmable depth parameter (MailboxDepth) with minimum depth of 2 entries

#### Data Transfer Operations
- Write Operations: Software-visible write data transmission through WRITE_DATA register to peer port
- Read Operations: Software-visible read data reception through READ_DATA register from peer port
- Bidirectional Data Flow: Simultaneous inbound and outbound channel operation for full-duplex communication
- Data Buffering: FIFO storage maintaining data ordering between write and read operations

#### Interrupt Generation and Handling
- Three Interrupt Types per Port:
  - Error Interrupt (EIRQ): Triggered on overflow or underflow conditions
  - Read Threshold Interrupt (RTIRQ): Triggered when read FIFO fill level exceeds programmable threshold
  - Write Threshold Interrupt (WTIRQ): Triggered when write FIFO fill level exceeds programmable threshold
- Programmable Threshold Configuration: Software-configurable thresholds via RIRQT and WIRQT registers
- Interrupt Enable Control: Per-interrupt type enable/disable via IRQEN register
- Interrupt Status Management: Sticky status bits requiring explicit software acknowledgment via IRQS register (write-1-to-clear)
- Interrupt Pending Logic: Hardware-generated pending status (IRQP) as bitwise AND of IRQS and IRQEN registers, recomputed combinationally on every read
- Dual Interrupt Outputs: Independent interrupt output signal for each of the two ports; driven by a dedicated `irq_driver` SC_METHOD process (single-writer pattern)
- Threshold Saturation Logic: Automatic reduction of threshold values exceeding FIFO depth to (MailboxDepth − 1)
- Retroactive Threshold Triggering: Writing a new threshold immediately re-evaluates the current FIFO fill level and may set IRQS bits without waiting for a new FIFO operation

#### Status Monitoring
- FIFO Empty Status: Indication when read FIFO has no available data (STATUS[0])
- FIFO Full Status: Indication when write FIFO cannot accept additional writes (STATUS[1])
- Write FIFO Level Status: Flag indicating write FIFO fill exceeds WIRQT threshold (STATUS[2])
- Read FIFO Level Status: Flag indicating read FIFO fill exceeds RIRQT threshold (STATUS[3])

#### Error Detection and Reporting
- Write-to-Full Error: Detection and reporting of write attempts to full FIFO; sets ERROR_FLAGS[1] and IRQS[2]
- Read-from-Empty Error: Detection and reporting of read attempts from empty FIFO; sets ERROR_FLAGS[0] and IRQS[2]
- Error Register Clear-on-Read: ERROR_FLAGS bits are cleared when the register is read; this does **not** clear IRQS[2] (EIRQ must be cleared separately via W1C on IRQS)

#### Control Operations
- Software-Controllable FIFO Flush: Per-FIFO flush capability via CTRL register (bit[0]=wflush, bit[1]=rflush)
- Flush Mechanism: `sc_fifo` has no `clear()` method; flush is implemented by draining entries with `nb_read()` in a loop
- Dual-Port Flush Coordination: Either port can flush either FIFO; flush commands from both ports take effect independently
- Asynchronous Reset Support: Active-low asynchronous reset (`rst_ni`) for initialization

#### AXI4-Lite Interface
- Dual AXI4-Lite Slave Ports: Two independent TLM target sockets (`tlm_target_socket<32>`)
- Register Access Control: Enforcement of read-only, write-only, and read-write register access types
- Invalid Access Response: Ignores for writes to read-only or reads 0 from write-only registers
- Configurable Base Addressing: Independent base address configuration for each port
- All Register Offsets: Fixed 8-byte aligned (64-bit register width)

#### Register Interface
- Control Registers: WRITE_DATA, READ_DATA, STATUS, ERROR_FLAGS, WIRQT, RIRQT, IRQS, IRQEN, IRQP, CTRL
- Register Reset Values: Defined reset state for all programmable registers (all 0x0)
- Reserved Bit Handling: Proper handling of reserved register fields (read as 0, writes ignored)
- Shadow State: All registers with functional side-effects maintain shadow state variables; reads return shadow state, not CSML backing store values

### 1.5 Is not list

The following features are excluded from the TLM model as they are not within transaction-level abstraction scope:

#### RTL Implementation Details
- FIFO Internal Implementation: Specific pointer management, memory structure, and RTL-level FIFO implementation details
- AXI Protocol State Machines: Detailed AXI handshaking sequences, internal state machine implementation
- Clock Domain Crossing: Physical clock domain crossing circuitry (model assumes single timing domain)
- Metastability Handling: Synchronization flip-flops and metastability resolution circuits

#### Physical and Electrical Characteristics
- Signal Timing: Setup time, hold time, clock-to-output delays
- Pin-Level Behavior: Physical signal transitions, drive strength, impedance
- Power Consumption: Dynamic and static power characteristics
- Voltage Levels: Logic level thresholds, voltage specifications

#### Test and Debug Features
- Test Mode Operation: Testmode enable signal not modeled
- DFT Structures: Scan chains, BIST logic, boundary scan
- Debug Interfaces: Any manufacturing test or debug-specific features not used in functional operation

#### Parametrization Details (RTL-Specific)
- AxiAddrWidth / AxiDataWidth: These RTL parameters are not constructor arguments in the TLM model; the model fixes all registers at 64-bit width (`regwidth=64`) and uses an 8-byte fixed stride for register offsets regardless of data bus width
- AXI Type Definitions: SystemVerilog-specific type definitions — TLM uses native SystemC types

#### Unimplemented/Reserved Functionality
- Reserved Register Bits: Bits marked as "Reserved" in register specifications
- Reserved Address Space: no registers are modeled at unassigned offsets. Accessing one is
  not ignored, though — it returns an error response, as the RTL raises `SLVERR`

#### Low-Level Protocol Details
- AXI Channel Arbitration: Internal arbitration between AW/W/B and AR/R channels
- Back-pressure Mechanisms: Physical ready/valid handshake implementation details
- Transaction Reordering: Any AXI-level transaction ID management (not applicable for AXI4-Lite)

As the model is not timing accurate it is not suitable for performance measurements.

## 2. Functional Description

### 2.1 Unit structure: channels behind one port

`axi_lite_mailbox_unit.sv` is not a single mailbox. It instantiates `NUM_MAILBOXES`
independent `axi_lite_mailbox` blocks behind one AXI-Lite slave port and demultiplexes
accesses to them by address. The model has the same two levels, and the rest of this
document describes the lower one unless it says otherwise.

| Class | Role |
|---|---|
| `mailbox_ip` | One channel: two ports, two FIFOs, one register block per port |
| `mailbox_unit_t<N>` | `N` channels, the address decode, and the interrupt vectors |

`mailbox_unit` is the `N = 8` typedef SEP uses, from `sep_pkg::NUM_MAILBOXES`. The template
parameter is the only difference between that and SMC's 32-channel wrapper
(`axil_mailbox_smc_wrap`), so the second instance is a one-line typedef rather than a
second model.

#### Address decode

The aperture is a flat array of `2 × N` register blocks of `0x800` bytes each, alternating
outbound and inbound (`axil_mailbox_sep_wrap.rdl`):

```
channel m outbound block @ m * 0x1000 + 0x000
channel m inbound  block @ m * 0x1000 + 0x800
```

SEP maps this at `0x10A0_0000 – 0x10A0_784F` (`0x7850` bytes). The window ends after the last
register of channel 7's inbound block — `7 × 0x1000 + 0x800 + 0x50` — rather than at the
`0x8000` the aperture would otherwise span, so the trailing gap is unmapped at the platform
bus and never reaches the unit.

`mailbox_unit_t::b_transport` divides the
incoming address by `MAILBOX_SIZE` to select a block, rewrites the address to be
block-relative, forwards to the channel, and restores the original address afterwards.
Because the channel only ever sees a block-relative address, its own decode and access
rules apply unchanged no matter which channel was selected. An address past the aperture
returns an error response.

#### Port naming and interrupt ordering

Outbound is port 0 and inbound is port 1, matching the RTL's
`slv_reqs_i({inbound_req, outbound_req})` ordering. The interrupt vectors follow the same
convention: `irq_o[0]` of channel *m* drives `outbound_irq_o[m]` and `irq_o[1]` drives
`inbound_irq_o[m]`. SEP wires the eight outbound lines to PIC sources 1–8.

Both ports are genuine bus targets. Neither side is a loopback stub, so an agent reaching
the inbound block over the bus sees the same register block the outbound side does — this
is what makes SEP → SMC delivery work rather than merely echoing locally.

### 2.2 Mailbox Config. Parameters

This section specifies the build-time configuration parameters for the Mailbox SystemC TLM model. These parameters are fixed at model instantiation time.

| Parameter Name | C++ Constructor Arg | Default | Description |
|---|---|---|---|
| `memory_size` | `unsigned int memory_size` | `0x50` (80 bytes) | TLM-specific: Total register space per port in bytes. Covers all 10 registers at 8 bytes each (offsets 0x00–0x48 + 8 = 0x50). Not an RTL parameter. |
| `MailboxDepth` | `unsigned int mailbox_depth` | `8` | Depth of the `sc_fifo<uint64_t>` channels between the two ports. Determines full/empty conditions, threshold saturation limit (max threshold = MailboxDepth − 1), and maximum data buffering capacity. Minimum value is 2. |
| `IrqEdgeTrig` | `bool irq_edge_trig` | `false` | Interrupt trigger mode. `false` = level-triggered (output held active while IRQP ≠ 0); `true` = edge-triggered (output pulses high then returns to inactive on 0→1 IRQP transition). |
| `IrqActHigh` | `bool irq_act_high` | `true` | Interrupt polarity. `true` = active-high (asserted = logic 1); `false` = active-low (asserted = logic 0). |

**Note:** The RTL parameters `AxiAddrWidth` and `AxiDataWidth` are **not** constructor arguments in the TLM model. All registers are fixed at 64-bit width with 8-byte aligned offsets.

#### Configuration Rules

1. `mailbox_depth` determines the saturation limit for threshold registers: if written value ≥ `mailbox_depth`, the stored value is clamped to `mailbox_depth − 1`.
2. `memory_size` must be ≥ `0x50` to cover all 10 registers (last register CTRL at offset 0x48 + 8 bytes = 0x50).
3. All threshold interrupt logic uses the saturated shadow value, not the raw written value.

### 2.3 Port interfaces

This section defines the SystemC TLM port interfaces for the Mailbox IP model.

#### Port Interface Definitions

| Interface Category | Port Name | Port Type | Description |
|---|---|---|---|
| Register Bus Interface | `socket0` | `tlm_target_socket<32>` | TLM target socket for Port 0. Hierarchically bound to `b0.target_socket`. Supports all read/write register transactions. Data written here flows through `fifo_0_to_1` and appears on Port 1's READ_DATA. |
| Register Bus Interface | `socket1` | `tlm_target_socket<32>` | TLM target socket for Port 1. Hierarchically bound to `b1.target_socket`. Data written here flows through `fifo_1_to_0` and appears on Port 0's READ_DATA. |
| Interrupt Outputs | `irq_o[0]` | `sc_out<bool>` | Interrupt output for Port 0. Driven exclusively by the `irq_driver` SC_METHOD. Asserts when IRQP[0..2] is non-zero; polarity and trigger mode determined by `IrqActHigh` and `IrqEdgeTrig`. |
| Interrupt Outputs | `irq_o[1]` | `sc_out<bool>` | Interrupt output for Port 1. Same behaviour as `irq_o[0]` for Port 1. |
| Clock Input | `clk_i` | `sc_in<double>` | Abstract clock frequency input in Hz. Used for timing-approximate modeling. Not a cycle-accurate clock edge signal. |
| Reset Input | `rst_ni` | `sc_in<bool>` | Active-low asynchronous reset. The `handle_reset` SC_METHOD is sensitive to `rst_ni.neg()`. When asserted (false), resets all registers, drains both FIFOs, clears all shadow state, and notifies `irq_driver` to de-assert interrupts. |

#### Internal Architecture

The model uses a **composition pattern**:
- `mailbox_base b0` / `b1`: Thin register containers (10 registers + CSML memory, no socket logic)
- `sc_fifo<uint64_t> fifo_0_to_1` / `fifo_1_to_0`: SystemC primitive FIFO channels (depth = `mailbox_depth`)
- All cross-port logic, shadow state, and `sc_fifo` management reside in `mailbox_ip`

FIFO routing (cross-connection):
- Port 0 writes → `fifo_0_to_1` → Port 1 reads
- Port 1 writes → `fifo_1_to_0` → Port 0 reads

#### Register Bus Interface Details

- **Bus Width:** 32-bit TLM socket (`tlm_target_socket<32>`); internally registers are 64-bit
- **Protocol:** TLM-2.0 generic payload with blocking transport interface
- **Burst Support:** No (single-beat transactions only)
- **Alignment:** All register offsets are 8-byte aligned (fixed 64-bit register width)

Register offsets relative to each port's base address:

| Register | Offset | Access |
|---|---|---|
| WRITE_DATA | 0x00 | WO |
| READ_DATA | 0x08 | RO |
| STATUS | 0x10 | RO |
| ERROR_FLAGS | 0x18 | RO |
| WIRQT | 0x20 | RW |
| RIRQT | 0x28 | RW |
| IRQS | 0x30 | RW |
| IRQEN | 0x38 | RW |
| IRQP | 0x40 | RO |
| CTRL | 0x48 | WO |

#### Interrupt Output Details

**Level-triggered mode (`IrqEdgeTrig = false`):**
- `irq_o[port]` = active level while `IRQP` is non-zero; inactive when `IRQP == 0`

**Edge-triggered mode (`IrqEdgeTrig = true`):**
- On 0→1 IRQP transition: pulse to active level, then immediately return to inactive level (same delta-cycle)
- When IRQP clears to 0: drive to inactive level

**Polarity (`IrqActHigh`):**
- `true`: active = logic 1, inactive = logic 0
- `false`: active = logic 0, inactive = logic 1

**Single-writer compliance:** `irq_o[port]` is written exclusively by the `irq_driver` SC_METHOD, which is triggered via `m_irq_update_event[port]` notifications from callbacks.

#### Reset Input Details

When `rst_ni` is asserted (logic 0):
- All 10 registers per port reset to RDL-defined values (0x0 for all)
- Both `sc_fifo` channels drained via `nb_read()` loop (no `clear()` in SystemC)
- All shadow state (error flags, IRQS, IRQEN, thresholds, edge detection) reset to 0/false
- `m_irq_update_event[0..1]` notified so `irq_driver` drives interrupts to inactive level

#### Excluded Ports

| RTL Port | Exclusion Reason |
|---|---|
| `test_i` | DFT signal, not part of functional operation |
| `base_addr_i[1:0]` | In TLM, base addresses are handled as constructor/integration configuration, not runtime ports |

#### SystemC Module Port Declaration

```cpp
class mailbox_ip : public sc_module {
public:
    // Port 0 and Port 1 register containers
    mailbox_base b0, b1;

    // TLM target sockets (32-bit bus width)
    tlm::tlm_target_socket<32> socket0;
    tlm::tlm_target_socket<32> socket1;

    // Bidirectional FIFOs
    sc_fifo<uint64_t> fifo_0_to_1;  // Port 0 outbound / Port 1 inbound
    sc_fifo<uint64_t> fifo_1_to_0;  // Port 1 outbound / Port 0 inbound

    // Interrupt Outputs
    sc_out<bool> irq_o[2];

    // Clock and Reset
    sc_in<double> clk_i;   // Abstract clock frequency (Hz)
    sc_in<bool>   rst_ni;  // Active-low reset

    SC_HAS_PROCESS(mailbox_ip);
    mailbox_ip(sc_module_name n,
               unsigned int memory_size  = 0x50,
               unsigned int mailbox_depth = 8,
               bool irq_edge_trig = false,
               bool irq_act_high  = true);
};
```

### 2.4 Memory-Mapped Registers

All registers are 64-bit wide (`regwidth=64`, `accesswidth=64`) with 8-byte aligned offsets. All reset values are 0x0.

#### Register Map Summary

| Register | Offset | Access | Reset | Description |
|---|---|---|---|---|
| WRITE_DATA | 0x00 | WO | 0x0 | Write data — enqueues to outbound FIFO |
| READ_DATA | 0x08 | RO | 0x0 | Read data — dequeues from inbound FIFO |
| STATUS | 0x10 | RO | 0x0 | Live FIFO status flags |
| ERROR_FLAGS | 0x18 | RO | 0x0 | Error condition flags (clear-on-read) |
| WIRQT | 0x20 | RW | 0x0 | Write FIFO interrupt threshold [7:0] |
| RIRQT | 0x28 | RW | 0x0 | Read FIFO interrupt threshold [7:0] |
| IRQS | 0x30 | RW | 0x0 | Interrupt status (write-1-to-clear) |
| IRQEN | 0x38 | RW | 0x0 | Interrupt enable mask |
| IRQP | 0x40 | RO | 0x0 | Interrupt pending = IRQS & IRQEN |
| CTRL | 0x48 | WO | 0x0 | FIFO flush control (self-clearing) |

#### Data Transfer Registers

**WRITE_DATA (0x00)** — Write-Only
- `write_data[63:0]`: 64-bit data to enqueue into the port's outbound `sc_fifo`
- If outbound FIFO is full (`num_free() == 0`): sets `ERROR_FLAGS[1]`, sets `IRQS[2]`
- On success: enqueues data, evaluates write threshold for current port and read threshold for peer port, updates IRQP/irq_o for both ports

**READ_DATA (0x08)** — Read-Only
- `read_data[63:0]`: 64-bit data dequeued from the port's inbound `sc_fifo`
- If inbound FIFO is empty (`num_available() == 0`): sets `ERROR_FLAGS[0]`, sets `IRQS[2]`
- On success: dequeues and returns data, updates IRQP/irq_o for both ports

#### Status and Error Registers

**STATUS (0x10)** — Read-Only, computed live from `sc_fifo` state

| Bit | Name | Description |
|---|---|---|
| [0] | empty | 1 = inbound FIFO is empty (`read_fifo.num_available() == 0`) |
| [1] | full | 1 = outbound FIFO is full (`write_fifo.num_free() == 0`) |
| [2] | write_level_above_thresh | 1 = outbound fill > WIRQT |
| [3] | read_level_above_thresh | 1 = inbound fill > RIRQT |
| [63:4] | — | Reserved, always 0 |

Fill level computation note:
- Read-side fill: `num_available()` (reflects same-delta dequeues)
- Write-side fill: `depth − num_free()` (reflects same-delta enqueues before `update()`)

**ERROR_FLAGS (0x18)** — Read-Only, clear-on-read

| Bit | Name | Reset | Description |
|---|---|---|---|
| [0] | read_error | 0x0 | 1 = attempted read from empty FIFO |
| [1] | write_error | 0x0 | 1 = attempted write to full FIFO |
| [63:2] | — | 0 | Reserved |

- Reading this register returns the current error state and **atomically clears** both bits
- Clear-on-read does **not** clear `IRQS[2]` (EIRQ); software must separately W1C that bit

#### Interrupt Configuration Registers

**WIRQT (0x20)** — Read-Write
- `wirqt[7:0]`: Write threshold (bits [63:8] reserved, ignore writes, read as 0)
- Write callback: extracts `[7:0]`, applies saturation (`≥ mailbox_depth → mailbox_depth − 1`), stores to `m_wirqt_threshold[port]`, immediately re-evaluates current write fill level
- Read callback: returns `m_wirqt_threshold[port]` (shadow, not CSML backing store)

**RIRQT (0x28)** — Read-Write
- `rirqt[7:0]`: Read threshold (same saturation and shadow-readback behaviour as WIRQT)
- Write callback: stores saturated value to `m_rirqt_threshold[port]`, re-evaluates inbound fill
- Read callback: returns `m_rirqt_threshold[port]` from shadow state

**IRQS (0x30)** — Read-Write (Write-1-to-Clear)

| Bit | Name | Description |
|---|---|---|
| [0] | wtirq | Write threshold exceeded (sticky) |
| [1] | rtirq | Read threshold exceeded (sticky) |
| [2] | eirq | Error condition occurred (sticky) |
| [63:3] | — | Reserved |

- **Write:** W1C — writing 1 to a bit clears it; writing 0 has no effect. Triggers IRQP recomputation.
- **Read:** Returns current shadow state from `m_irqs_*[port]` variables (not CSML backing store)
- Bits are set by hardware (callbacks) regardless of IRQEN state
- Writing to IRQS does **not** auto-re-set any bit; hardware re-sets only on new error/threshold events

**IRQEN (0x38)** — Read-Write

| Bit | Name | Description |
|---|---|---|
| [0] | wtirq | 1 = write threshold IRQ enabled |
| [1] | rtirq | 1 = read threshold IRQ enabled |
| [2] | eirq | 1 = error IRQ enabled |
| [63:3] | — | Reserved |

- **Write:** Updates `m_irqen_*[port]` shadow state; immediately recomputes IRQP and updates `irq_o`
- **Read:** Returns shadow state from `m_irqen_*[port]` (not CSML backing store)

**IRQP (0x40)** — Read-Only, combinationally computed: `IRQP = IRQS & IRQEN`

| Bit | Name | Description |
|---|---|---|
| [0] | wtirq | Write threshold IRQ pending |
| [1] | rtirq | Read threshold IRQ pending |
| [2] | eirq | Error IRQ pending |
| [63:3] | — | Reserved |

- Recomputed on every read from `m_irqs_*` and `m_irqen_*` shadow variables
- OR of all bits drives `irq_o[port]` via the `irq_driver` SC_METHOD

#### Control Register

**CTRL (0x48)** — Write-Only, self-clearing

| Bit | Name | Description |
|---|---|---|
| [0] | wflush | 1 = flush outbound FIFO (write FIFO for this port) |
| [1] | rflush | 1 = flush inbound FIFO (read FIFO for this port) |
| [63:2] | — | Reserved, writes ignored |

- Self-clearing: register always reads as 0x0 (WO — reads 0)
- Flush implementation: SystemC `sc_fifo` has no `clear()` method; drain is performed by calling `nb_read()` in a loop until `num_available() == 0`
- wflush semantics: flushes `write_fifo_for(port)` — this is the peer port's inbound FIFO, so peer's STATUS[0] (empty) becomes 1 and STATUS[3] (read threshold) clears
- rflush semantics: flushes `read_fifo_for(port)` — this is the peer port's outbound FIFO, so peer's STATUS[1] (full) clears and STATUS[2] (write threshold) clears
- After flush, IRQP/irq_o updated for both ports

#### Register Access Error Handling

Every refused access returns a TLM error response. The model does not silently absorb
anything, because the RTL raises `SLVERR` on the AXI-Lite bus and firmware is entitled to
see the fault.

| Condition | Response |
|---|---|
| Access to an unmapped offset within a block (outside `0x00..0x4F`) | data = 0, error response |
| Access past the unit aperture (block index ≥ `2 × N`) | error response, raised by `mailbox_unit_t` before the channel is reached |
| Write to RO register (READ_DATA, STATUS, ERROR_FLAGS, IRQP) | error response |
| Read from WO register (CTRL) | data = 0, error response |
| Read from WRITE_DATA | data = `0xFEEDC0DE`, **OKAY** — see below |
| Write to full FIFO (WRITE_DATA when full) | sets `ERROR_FLAGS[1]` + `IRQS[2]`, error response |
| Read from empty FIFO (READ_DATA when empty) | data = `0xFEEDDEAD`, sets `ERROR_FLAGS[0]` + `IRQS[2]`, error response |

Two sentinel values are returned instead of zero, both matching RTL:

- **`0xFEEDC0DE`** on a read of `WRITE_DATA`. This is the one write-only read the RTL
  permits, and it completes with OKAY rather than an error — the only refused-looking
  access that is not a fault.
- **`0xFEEDDEAD`** on a read of `READ_DATA` when the FIFO is empty, alongside the error
  response and the error flag.

Firmware can therefore tell an empty mailbox from a mailbox that legitimately held zero,
which is the reason the sentinels exist.

## 3. Use model

Mailbox will be modeled at LT abstraction level with timing annotation and temporal decoupling. The intended use case of this model is software development. The model provides FIFO-based bidirectional communication channels for inter-processor or inter-chiplet communication using abstracted AXI4-Lite interfaces.

### 3.1 Callbacks on Memory-mapped registers/bit-fields

This section documents all register callbacks registered in the `mailbox_ip` constructor. Callbacks are needed when register access triggers functional side-effects beyond simple CSML storage.

#### Registers Requiring Write Callbacks

| Callback | Trigger | Side-Effects |
|---|---|---|
| `handle_write_WRITE_DATA(port)` | Write to WRITE_DATA | Pre-check `num_free()`; if full: set `ERROR_FLAGS[1]`, `IRQS[2]`, return error. If space: `nb_write()` to outbound sc_fifo, evaluate write threshold for current port and read threshold for peer, update IRQP/irq_o for both ports |
| `handle_write_WIRQT(port)` | Write to WIRQT | Extract bits [7:0]; saturate if ≥ mailbox_depth; store to `m_wirqt_threshold[port]`; immediately compare write fill level: if fill > threshold, set `IRQS[0]`; update IRQP/irq_o |
| `handle_write_RIRQT(port)` | Write to RIRQT | Extract bits [7:0]; saturate if ≥ mailbox_depth; store to `m_rirqt_threshold[port]`; immediately compare read fill level: if fill > threshold, set `IRQS[1]`; update IRQP/irq_o |
| `handle_write_IRQS(port)` | Write to IRQS | W1C: for each bit [0..2], if write_data bit = 1, clear corresponding `m_irqs_*` shadow variable. Update IRQP/irq_o. |
| `handle_write_IRQEN(port)` | Write to IRQEN | Update `m_irqen_wtirq/rtirq/eirq[port]` shadow variables from bits [2:0]. Update IRQP/irq_o immediately (retroactive enable). |
| `handle_write_CTRL(port)` | Write to CTRL | Extract wflush[0] and rflush[1]. Drain `write_fifo_for(port)` via `nb_read()` loop if wflush=1. Drain `read_fifo_for(port)` via `nb_read()` loop if rflush=1. Update IRQP/irq_o for both ports. |

#### Registers Requiring Read Callbacks

| Callback | Trigger | Return Value / Side-Effects |
|---|---|---|
| `handle_read_READ_DATA(port)` | Read from READ_DATA | Pre-check `num_available()`; if empty: set `ERROR_FLAGS[0]`, `IRQS[2]`, return error. If data: `nb_read()` from inbound sc_fifo, return value, update IRQP/irq_o for both ports. |
| `handle_read_STATUS(port)` | Read from STATUS | Compute live status from sc_fifo state: bits [0]...[3] from `rfill`, `wfill`, threshold comparisons. No side-effects. Required because STATUS is volatile (not stored). |
| `handle_read_ERROR_FLAGS(port)` | Read from ERROR_FLAGS | Return bits from `m_error_flag_read_error/write_error[port]`, then **atomically clear** both shadow bits. Does **not** clear `IRQS[2]`. |
| `handle_read_WIRQT(port)` | Read from WIRQT | Returns `m_wirqt_threshold[port]` (saturated shadow value, not CSML backing store). Required because write callback stores saturated value to shadow only. |
| `handle_read_RIRQT(port)` | Read from RIRQT | Returns `m_rirqt_threshold[port]` (saturated shadow value). Same rationale as WIRQT read callback. |
| `handle_read_IRQS(port)` | Read from IRQS | Returns current shadow state: bits [0..2] from `m_irqs_wtirq/rtirq/eirq[port]`. Required because write callback updates shadow only. |
| `handle_read_IRQEN(port)` | Read from IRQEN | Returns current shadow state: bits [0..2] from `m_irqen_wtirq/rtirq/eirq[port]`. Required because write callback updates shadow only. |
| `handle_read_IRQP(port)` | Read from IRQP | Computes `IRQP = IRQS & IRQEN` combinationally from shadow variables. Drives irq_o via irq_driver. |

#### Registers Excluded from Callbacks

| Register | Reason |
|---|---|
| WRITE_DATA (read) | WO register — reads return error (access-type violation, no functional callback) |
| READ_DATA (write) | RO register — writes return error |
| CTRL (read) | WO register — reads return error |

#### Implementation Notes

**Dual-Port Architecture:**
- Each port has its own register set at a separate base address
- Cross-port effects: `WRITE_DATA` on Port 0 enqueues to `fifo_0_to_1` which Port 1 reads via `READ_DATA`
- FIFO routing: `write_fifo_for(p) = (p==0) ? fifo_0_to_1 : fifo_1_to_0`; `read_fifo_for(p) = (p==0) ? fifo_1_to_0 : fifo_0_to_1`

**Interrupt Propagation Chain:**
1. Hardware (callbacks) sets `m_irqs_*[port]` on error/threshold conditions
2. IRQP computed on demand: `IRQP = IRQS & IRQEN` (from shadow variables)
3. `update_irqp_and_output(port)` notifies `m_irq_update_event[port]`
4. `irq_driver()` SC_METHOD (single writer) computes IRQP, applies polarity/trigger-mode logic, writes to `irq_o[port]`

**Write Fill Level vs. Read Fill Level:**
- Write-side threshold comparison uses `depth − num_free()` to account for same-delta writes (before `sc_fifo::update()` propagates)
- Read-side threshold comparison uses `num_available()` which correctly reflects same-delta `nb_read()` calls

**IRQS Sticky Behaviour:**
- IRQS bits are only cleared by explicit W1C software write
- Threshold IRQS bits are NOT automatically cleared when the FIFO level drops back below threshold
- Error IRQS bit (`IRQS[2]`) is NOT cleared by reading ERROR_FLAGS — it must be cleared via W1C

**Self-Clearing CTRL:**
- CTRL register has no persistent storage; always reads as error (WO)
- The write callback executes the flush synchronously within the TLM transaction and returns; no deferred state

## 4. Assumptions

The following assumptions apply to the Mailbox SystemC TLM model:

### TLM Abstraction Level
- The model operates at Loosely Timed (LT) abstraction level with timing annotation and temporal decoupling
- Intended use case is software development, not performance measurement or cycle-accurate simulation
- Model separates behavior, communication, and timing as far as possible within TLM constraints

### Timing Behavior
- Clock input (`clk_i`) represents abstract frequency in Hz, not cycle-accurate clock edges
- Timing annotations are approximate, not cycle-accurate
- Interrupt latency may be approximated based on clock frequency
- FIFO access delays are functional, not timing-accurate
- No support for cycle-accurate performance measurements

### FIFO Ordering and Behavior
- FIFO data ordering (First-In-First-Out) is strictly maintained via `sc_fifo<uint64_t>`
- Each direction uses an independent `sc_fifo` primitive channel; no cross-channel interference
- Cross-port data flow: Port 0 WRITE_DATA → `fifo_0_to_1` → Port 1 READ_DATA; and vice versa
- FIFO depth fixed at instantiation (minimum 2); `sc_fifo` does not support runtime resize
- Threshold comparisons occur immediately after any FIFO state change or threshold write (retroactive)

### Register Access and Protocol
- All register accesses complete atomically (blocking transport interface)
- Register access type enforcement (RO/WO/RW) is strictly modeled
- Reserved register bits read as 0 and ignore writes
- AXI4-Lite protocol abstracted to TLM-2.0 generic payload
- No burst transactions; no byte enable modeling
- All register offsets are 8-byte aligned regardless of RTL bus width parameters

### Reset Protocol
- Reset signal (`rst_ni`) is active-low asynchronous
- Sensitive to `rst_ni.neg()` — fires SC_METHOD on falling edge
- Drains both `sc_fifo` channels using `nb_read()` loop (no `clear()` available)
- All registers and shadow state reset to 0x0; all IRQS/IRQEN/threshold/error flags cleared
- Interrupt outputs de-asserted via `m_irq_update_event` notification

### Interrupt Behavior
- `irq_driver` is the sole writer to `irq_o[port]` signals (single-writer SC_METHOD pattern)
- Interrupt assertions and de-assertions use event-driven notification: callbacks notify `m_irq_update_event[port]`; `irq_driver` handles the actual signal write
- Level-triggered mode: `irq_o[port]` held active while any IRQP bit is set
- Edge-triggered mode: single pulse (active→inactive in same delta-cycle) on 0→1 IRQP transition
- IRQS bits are sticky; they do not auto-clear when the triggering condition clears
- IRQS[2] (EIRQ) is cleared only by explicit W1C; reading ERROR_FLAGS does not clear it

### Error Handling
- ERROR_FLAGS bits set on failed FIFO operations; cleared atomically on register read (clear-on-read)
- ERROR_FLAGS clear does **not** clear `IRQS[2]` — software must separately W1C IRQS
- `IRQS[2]` is set independently by both overflow and underflow; bits accumulate by OR

### Cross-Port Timing and Coordination
- No clock-cycle timing relationship between Port 0 and Port 1 operations
- Flush from either port affects the shared `sc_fifo`; both ports observe state change immediately within TLM abstraction
- Simultaneous access to different registers on different ports is allowed (independent register spaces)
- No metastability or clock domain crossing modeled

### Software Behavior Assumptions
- Software should check STATUS register before FIFO operations to avoid error responses
- Software must explicitly clear IRQS bits via W1C; hardware re-sets bits only on new events
- Threshold registers should be programmed before enabling interrupts
- Reads of WIRQT/RIRQT return the saturated value, not the originally written value (if saturation applied)
- Base addresses for each port are configured at integration/instantiation time

### Configuration and Instantiation
- `mailbox_depth` ≥ 2 (minimum to ensure at least one buffered entry and valid saturation)
- `memory_size` ≥ 0x50 (must cover all 10 registers at 8-byte stride)
- `AxiAddrWidth` and `AxiDataWidth` are not modeled as constructor parameters; register width is fixed at 64 bits
- Total constructor parameters: 4 (`memory_size`, `mailbox_depth`, `irq_edge_trig`, `irq_act_high`)
- Channel count is a template parameter on `mailbox_unit_t`, not a constructor argument, so it
  is fixed at compile time exactly as `NUM_MAILBOXES` is in RTL. SEP uses 8; a different
  subsystem changes only the typedef

### Excluded Features (Not Modeled)
- Test mode operation (`test_i` signal)
- Physical signal timing (setup/hold times, clock-to-output delays)
- Pin-level behavior (drive strength, impedance, voltage levels)
- Power consumption
- Clock domain crossing circuitry
- Metastability handling and synchronization
- AXI protocol state machines (handshaking abstracted to TLM)
- FIFO internal implementation details (RTL pointer management, memory array)
- Transaction reordering
- Burst transactions

---

**Document Metadata:**
- IP Name: Mailbox
- RTL Module Names: `axi_lite_mailbox` (channel), `axi_lite_mailbox_unit` (unit)
- TLM Model Classes: `mailbox_ip` (channel), `mailbox_unit_t<N>` (unit; `mailbox_unit` is `N = 8` for SEP)
- Last Updated: 2026-08-23
- TLM Abstraction Level: Loosely Timed (LT)
- Total Registers: 10 per port (all 64-bit wide, 8-byte stride)
- Channel Port Interfaces: 6 (2 TLM sockets, 2 interrupt outputs, 1 clock, 1 reset)
- Unit Port Interfaces: 1 TLM socket, `2 × N` interrupt outputs, 1 clock, 1 reset
- Total Constructor Parameters: 4 (`memory_size`, `mailbox_depth`, `irq_edge_trig`, `irq_act_high`)
- Dual-Port Architecture: Yes — `b0`/`b1` register containers + `fifo_0_to_1`/`fifo_1_to_0`
- Primary Use Case: Software development and validation
- RTL comparison and change history: `md_files/MAILBOX_RTL_VS_VP.md`
