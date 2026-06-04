# UART PC16550D – High-Level Design Document

## TABLE OF CONTENTS

1. Introduction  
1.1 Objective  
1.2 Scope  
1.3 Acronyms  
1.4 Is list  
1.5 Is not list  

2. Functional Description  
2.1 UART Configuration Parameters  
2.2 Port Interfaces  
2.3 Memory-Mapped Registers  
2.4 Temporal Decoupling

3. Use Model  
3.1 Callbacks on Memory-Mapped Registers / Bit-fields  
3.2 CCI Configuration

4. Assumptions  

---

## 1. Introduction

### 1.1 Objective

This document provides the design specifications for the 16550 UART as a SystemC TLM2 compliant model. The model is implemented at Loosely Timed (LT) abstraction level and is intended for software development and validation. The design focuses on functional UART behavior such as FIFOs, register interactions, and interrupt handling while abstracting away cycle-accurate serial timing and electrical characteristics.

The UART model converts parallel data to serial format on transmit and serial data to parallel format on receive, providing buffered communication through transmit and receive FIFOs with interrupt support.

### 1.2 Scope

The document describes the design and implementation details of the UART 16550 model. It focuses on register-level behavior, FIFO handling (including extended trigger levels), interrupt logic (including test-forced interrupts), loopback functionality, and reset handling. External modem signaling, cycle-accurate baud timing, and electrical signaling are outside the scope of this LT model.

### 1.3 Acronyms

UART - Universal Asynchronous Receiver/Transmitter  
FIFO - First In First Out  
LT - Loosely Timed  
TLM - Transaction Level Modeling  
THR - Transmitter Holding Register  
RBR - Receiver Buffer Register  
LSR - Line Status Register  
IER - Interrupt Enable Register  
IIR - Interrupt Identification Register  
DMA - Direct Memory Access  
TSR - Transmit Shift Register  
ECR - Enhanced/Extended Control Register (RCVR trigger MSBs)  
ITR - Interrupt Test Register (force interrupts)
CCI - Configuration, Control, and Inspection
QK  - Quantum Keeper

### 1.4 Is list

The following features shall be modeled:

- 4096-byte Transmit and Receive FIFOs  
- Transmit and Receive datapath behavior (THR/RBR, FIFO paths)  
- FIFO Control Register (FCR) and Extended trigger level via ECR  
- Interrupt system including IER, IIR, ITR (test-forced), and INTR signal  
- THRE, RDA, Line Status, Modem Status, FIFO Error, Character Timeout (via ITR)  
- Loopback modes: internal loopback (`MCR.LOOP`) and line loopback (`MCR.LINE_LOOPBACK`) with precedence handling  
- UART reset behavior  
- Abstracted serial data injection through custom SystemC interface  
- Register-level software-visible behavior, including alias via `LCR.DLAB`  
- Overrun Error (OE) modeling in non-FIFO and FIFO-full scenarios
- **CCI-based configuration** for simulation parameters
- **Temporal Decoupling** using TLM Quantum Keeper for performance optimization

### 1.5 Is not list

The following are not modeled:

- Cycle-accurate baud rate generation and bit-level timing  
- Full modem control signaling behavior (RTS, CTS, DSR, DTR, RI, DCD) beyond minimal MSR/IIR interactions  
- Parity generation/detection and character framing  
- Break detection and generation  
- DMA handshake signaling  
- Framing/Parity/Break error bits beyond OE in LSR  
- RX timeout timing (only test-forced via ITR)  
- Electrical and waveform characteristics

---

## 2. Functional Description

### 2.1 UART Configuration Parameters

The UART model operates with fixed configuration aligned to a functional 16550-style device:

- Transmit FIFO Depth: 4096 bytes  
- Receive FIFO Depth: 4096 bytes  
- Data width: 8 bits  
- Word length, parity, stop bits are not functionally modeled (LCR used for DLAB and minimal control only)  
- FIFO mode controlled via `FCR[0]` with extended RCVR trigger levels combined from `ECR` MSBs and `FCR` LSBs (levels supported: 1, 4, 8, 14, 32, 64, 128, 256, 512, 1024, 2048, 4096)

These parameters are functionally abstracted and not dynamically configurable via the model build process.

#### Timing Parameters

The model uses simple wait statements for functional timing approximation:

- `m_byte_time`: 1ns - Wait time in `send_byte()` to simulate byte transmission time (default)

These parameters provide functional timing behavior without cycle-accurate modeling.

### 2.2 Port Interfaces

#### TLM Register Interface
Provides memory-mapped register access for control and status operations.

- `tlm_utils::simple_target_socket` in the UART  
- Supports read/write of registers via `b_transport`

#### Native UART Interface
SystemC port/export interface for bidirectional communication with the terminal model:

**Interface Definition (`terminal_if`)**:
- `virtual void uart_to_terminal(uint8_t data)`: Called by UART to send data to terminal
- `virtual void terminal_to_uart(uint8_t data)`: Called by Terminal to send data to UART

**Ports (UART Side)**:
- `sc_port<terminal_if> terminal_port`: Port used by UART to send data to Terminal (binds to Terminal's export)
- `sc_export<terminal_if> terminal_export`: Export provided by UART to receive data from Terminal (Terminal binds its port here)

**Ports (Terminal Side)**:
- `sc_port<terminal_if> uart_port`: Port used by Terminal to send data to UART (binds to UART's export)
- `sc_export<terminal_if> uart_export`: Export provided by Terminal to receive data from UART (UART binds its port here)

#### Reset Signal
`sc_in<bool> reset` – Active high reset input.

#### Interrupt Signal
`sc_out<bool> INTR` – UART interrupt output line monitored by the testbench.

#### Terminal Interface
A terminal emulator module provides bidirectional character interaction via a TCP socket. A Python client connects to the server and displays/forwards characters, simulating a serial terminal. The UI can auto-spawn the client.

---

### 2.3 Memory-Mapped Registers

Register grouping based on features (software-visible behavior):

Data Transfer and Status
- RBR – Receiver Buffer Register (read; non-FIFO or bytes popped from RCVR FIFO)  
- THR – Transmitter Holding Register (write; pushes to TX FIFO/non-FIFO path)  
- LSR – Line Status Register (read; DR, THRE, TEMT; clears certain error bits on read; OE modeled)

FIFO and Trigger Control
- FCR – FIFO Control Register (write; FIFO enable, RX/TX resets, trigger LSBs)  
- ECR – Extended Control Register (write; provides MSBs for extended RCVR FIFO trigger level)

Interrupts
- IER – Interrupt Enable Register (read/write; enables RDA, THRE, ELSI, etc.)  
- IIR – Interrupt Identification Register (read; reports highest-priority pending interrupt)  
- ITR – Interrupt Test Register (write; forces specific interrupt conditions for test)

Line/Modem Control
- LCR – Line Control Register (read/write; DLAB used for DLL/DLM access; other formatting fields not modeled)  
- MCR – Modem Control Register (read/write; supports `LOOP` and `LINE_LOOPBACK`)  
- MSR – Modem Status Register (stubbed)

Baud Rate and Miscellaneous
- DLL/DLM – Divisor Latch Registers (present; accessed when `LCR.DLAB=1`; no timing effect)  
- SCR – Scratch Register (optional/stubbed behavior)

Aliasing and Offsets
- Offset aliasing between RBR/THR/DLL/DLM follows `LCR.DLAB` behavior.  
- Internal callback registration maps: `IER@0x04`, `FCR/IIR@0x08`, `LSR@0x14` in the current model implementation.

Interrupt Behavior Summary
- Sources: RDA (RX data ready by FIFO-trigger or DR), THRE, Line Status (e.g., OE). Modem Status (stub), FIFO Error and Character Timeout (test-forced).  
- Priority: Test-forced (`ITR`) highest; then Line Status > RX Data Ready > THR Empty(as per the spec, but considering interrupts only that are modelled).  
- Example semantics: THRE may clear on IIR read; DR clears on RBR read.

### 2.4 Temporal Decoupling

The model implements temporal decoupling using the `tlm_utils::tlm_quantumkeeper` to improve simulation performance by reducing the number of context switches (calls to `wait()`).

- **Mechanism**: The model accumulates local time offset (`m_qk.inc()`) during data transmission/reception instead of immediately calling `wait()`.
- **Synchronization**: The model synchronizes with the SystemC kernel (`m_qk.sync()`) only when the accumulated local time exceeds the global quantum.
- **Conditional Activation**:
    - If the global quantum is set (via CCI), temporal decoupling is active.
    - If the global quantum is not set (default), the quantum keeper effectively behaves as if disabled (syncs rarely or never), resulting in functional timing behavior.

---

## 3. Use Model

The UART is modeled at LT abstraction level focusing on register-driven operational behavior. The testbench interacts through the TLM socket for register control and through the native UART interface for data exchange with the terminal. The terminal UI enables interactive RX/TX through a TCP-connected client.

### 3.0 SystemC Processes

The UART model uses several SystemC processes for concurrent operation:

**tx_process()**  
Handles transmission from THR/TX FIFO to the terminal. Triggered by `tx_process_event` when data is written to THR. Reads from TX FIFO (if enabled) or THR, sends via `send_byte()`, and updates `LSR.THRE` and `LSR.TEMT`.

**rx_process()**  
Updates the Data Ready (`LSR.DR`) bit and interrupt status when RX data arrives. Triggered by `rx_process_event` after data is written to RBR/RX FIFO.

**terminal_to_uart() (Callback)**  
Invoked directly by the terminal interface when data is received. Writes data to RBR or RX FIFO based on FIFO mode and handles overrun error (`LSR.OE`) when buffer/FIFO is full. Triggers `rx_process` via `rx_process_event` to update status/interrupts.

**reset_method()**  
Sensitive to rising edge of reset signal. Clears all registers, FIFOs, internal state, deasserts INTR, and re-evaluates interrupt status.

### 3.1 Callbacks on Memory-Mapped Registers / Bit-fields

Key behavioral callbacks include:

- Write to THR  
Pushes data into TX FIFO or single buffer and triggers the transmit process; sets `LSR.THRE` when holding register becomes empty.

- Read from RBR  
Pops data from RCVR FIFO or single buffer; clears `LSR.DR` when emptied.

- Write to FCR / ECR  
Controls FIFO enable, clears FIFOs, and sets RCVR FIFO trigger (ECR provides MSBs; FCR provides LSBs). Self-clearing reset bits handled.

- Write to IER  
Enables/disables interrupt sources (e.g., ERBFI/RDA, ETBEI/THRE, ELSI/Line Status).

- Read from IIR  
Reports highest-priority pending interrupt; certain side-effects are modeled (e.g., THRE clear on read path as per model).

- LSR updates  
`THRE` and `DR` set/clear based on holding/FIFO state and RX/TX activity; `OE` is modeled for overrun in non-FIFO and FIFO overflow conditions; reading `LSR` clears error bits per spec coverage.

- Write to ITR  
Forces interrupts (RDA, THRE, Line Status, Modem Status, FIFO Error, Character Timeout) to validate interrupt priority/servicing.

Interrupt clearing behavior based on spec/model:  
- THRE may clear on IIR read or THR write (per modeled flow)  
- DR cleared on RBR read  
- Line Status error bits cleared on LSR read (OE implemented; others stubbed)

### 3.2 CCI Configuration

The model supports configuration via the Accellera CCI (Configuration, Control, and Inspection) standard.

**Parameters:**
- `TimeKeeperQuantumNs` (double): Sets the global quantum for temporal decoupling in nanoseconds.
    - Default: `0.0` (Disabled / Functional Timing)
    - If set > 0.0: Enables temporal decoupling with the specified quantum.

**JSON Configuration:**
The testbench supports loading CCI parameters from a JSON configuration file.
Example `config.json`:
```json
{
  "testbench": {
    "uart_inst": {
      "TimeKeeperQuantumNs": 1000.0
    }
  }
}
```

**Usage:**
```bash
./uart_test [config_file.json]
```

---

## 4. Assumptions

### Transaction-Level Behavior

All register accesses are handled as atomic TLM transactions through `b_transport`. No clock-cycle accuracy is modeled. FIFO operations produce immediate functional side effects.

### Interrupt Handling

Interrupts are asserted when enabled conditions occur or when forced via `ITR`. Priority is enforced as follows (highest to lowest in model):

1. Test-forced interrupts (`ITR`)  
2. Receiver Line Status  
3. Received Data Ready (FIFO trigger or DR)  
4. THRE (Transmitter Holding Register Empty)  

Interrupt state is latched and read via `IIR`. Clearing follows 16550-style rules as approximated by the model.

### FIFO Operation

- FIFO disabled: Single-byte buffers used (RBR/THR paths)  
- FIFO enabled: 4096-byte `sc_fifo` used for RX/TX  
- When RX buffer/FIFO overflows, new data may be dropped and `LSR.OE` set; FIFO reset clears contents and status bits as modeled.

### Data Path Abstraction

No bit-level serial waveform modeling. Each byte represents one completed serial character.

### Reset Behavior

Reset clears:
- FIFOs  
- All internal buffers  
- Registers to default values  
- Deasserts interrupt output and re-evaluates interrupt state

### Excluded Timing Features

- Baud clock edges not modeled  
- RX timeout not modeled as a timed feature (supported as test-forced via `ITR`)  
- Trigger-level timing not simulated

### Testing Approach

The UART and terminal models are tested separately with dedicated testbenches:

**uart_test**  
Tests UART functionality including register access, FIFO operations, interrupts, loopback modes, and overrun error handling. Uses TLM socket for register access and `terminal_to_uart()` method calls via the `terminal_port` to inject RX data. Supports CCI-based configuration for performance tuning.

**terminal_test**  
Interactive testbench for the terminal UI model (`uart_terminal_ui`). Auto-spawns an gnome-terminal window, sends a welcome message, and echoes characters typed in the terminal back to the screen. Validates bidirectional communication through the native UART interface.

---