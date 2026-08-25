# DMA Controller High-Level Design Document

## TABLE OF CONTENTS

1. [Introduction](#1-introduction)
   - [1.1 Objective](#11-objective)
   - [1.2 Scope](#12-scope)
   - [1.3 Acronyms](#13-acronyms)
   - [1.4 Is list](#14-is-list)
   - [1.5 Is not list](#15-is-not-list)

2. [Functional Description](#2-functional-description)
   - [2.1 DMA Config. Parameters](#21-dma-config-parameters)
   - [2.2 Port interfaces](#22-port-interfaces)
   - [2.3 Memory-Mapped Registers](#23-memory-mapped-registers)

3. [Use model](#3-use-model)
   - [3.1 Callbacks on Memory-mapped registers/bit-fields](#31-callbacks-on-memory-mapped-registersbit-fields)

4. [Assumptions](#4-assumptions)

---

## 1. Introduction

### 1.1 Objective

This document provides the design specifications for the DMA Controller (DMA) as a SystemC TLM2 compliant model. This will be modeled at LT abstraction level with timing annotation and temporal decoupling. The intended use case of this model is software development. In this design we shall try to separate behavior, communication and timing as far as possible. The functional description of DMA along with the information about the internal registers and interface ports are discussed in detail.

### 1.2 Scope

The scope of the document is to describe the design details of the DMA Controller. It mainly focuses on the implementation level details of the DMA Controller that in turn would form the basis for developing the DMA LT model. Also, the interface details are discussed which would be used for communicating with the outside world.

### 1.3 Acronyms

| Acronym | Definition |
| --- | --- |
| **LT** | Loosely Timed |
| **TLM** | Transaction-Level Modeling |
| **DMA** | Direct Memory Access |
| **TL-UL** | TileLink Uncached Lightweight |
| **ASID** | Address Space Identifier |
| **OT** | OpenTitan |
| **CTN** | Control Network |
| **SYS** | System Bus |
| **RACL** | Resource Access Control List |
| **SHA** | Secure Hash Algorithm |
| **FIFO** | First-In-First-Out |
| **RW** | Read/Write |
| **RO** | Read Only |
| **WO** | Write Only |
| **RW1C** | Read/Write-1-to-Clear |
| **RW0C** | Read/Write-0-to-Clear |

### 1.4 Is list

The DMA Controller TLM model includes the following software-visible functional features:

#### Data Transfer Capabilities
- Single-channel DMA operation processing one transaction at a time
- Memory-to-Memory transfer mode
- Memory-to-Peripheral transfer mode
- Peripheral-to-Memory transfer mode
- Configurable transfer granularity (1-byte, 2-byte, 4-byte data widths)
- Sub-word extraction and replication for bus transfers
- Chunked data transfer mechanism with configurable chunk size

#### Addressing Modes
- Incrementing address mode (auto-increment after each transfer)
- Fixed address mode (constant address for FIFO-like peripherals)
- Wrapping/Circular buffer address mode (wrap to chunk start address)
- Independent source and destination addressing configuration
- 64-bit address space support for System bus interface
- 32-bit address space for OT internal and CTN interfaces

#### Bus Interface Support
- Three bus interface operation (OT Private, CTN, System)
- TileLink Uncached Lightweight (TL-UL) protocol for OT and CTN interfaces
- Custom 64-bit System bus protocol
- Single outstanding read or write request behavior
- Bus master functionality for autonomous memory access

#### Hardware Handshaking Mode
- Hardware handshake trigger mechanism for peripheral FIFO service
- Interrupt-based trigger mechanism for peripherals
- Dedicated hardware signal support for I2C, UART, SPI Device, SPI Host
- Configurable interrupt acknowledgment mechanism
- Hardware handshake enable/disable control

#### Interrupt Generation
- Transfer completion interrupt (dma_done)
- Chunk completion interrupt (dma_chunk_done)
- Error condition interrupt (dma_error)
- Status-type interrupt implementation
- Interrupt clearing via status register write

#### Inline SHA-2 Hashing
- SHA-256 hash computation during data transfer
- SHA-384 hash computation during data transfer
- SHA-512 hash computation during data transfer
- Hash state initialization control (initial_transfer bit)
- Digest byte-swap endianness control
- Digest output available in SHA2_DIGEST registers
- Hash streaming across multiple transfers

#### Security Features
- Memory range validation for OT internal access
- Address Space Identifier (ASID) based access control
- Memory region isolation between OT Private, OT DMA, and SoC memory
- Multi-bit encoded security register values
- Write-once register locking (RANGE_REGWEN)
- Hardware-managed configuration locking during operation (CFG_REGWEN)
- RACL role-based access control for system bus

#### Error Detection and Reporting
- Source address alignment error detection
- Destination address alignment error detection
- Invalid opcode detection
- Invalid transfer size detection
- Bus error detection and propagation
- Memory range violation detection
- ASID validation error detection
- Comprehensive error code register (ERROR_CODE)

#### Transfer Control and Status
- Software-initiated transfer control (go bit)
- Transfer abort capability (abort bit)
- DMA busy/idle state indication
- Transfer done status
- Chunk done status
- Error status indication
- Aborted transfer status
- Configuration register locking during operation
- Status register polling capability

#### Functional Timing
- Transfer completion notification timing
- Chunk completion notification timing
- Hardware handshake response timing to peripheral interrupts
- Sequential chunk processing behavior
- Transfer abort immediate response

### 1.5 Is not list

The following implementation-specific details are excluded from the TLM model:

#### Physical/Electrical Characteristics
- Actual clock frequency specifications (functional clock gating is modeled, but not electrical timing)
- Reset signal electrical characteristics (asynchronous reset implementation details)
- Signal voltage levels and current drive specifications
- Pin capacitance and loading characteristics
- Power consumption measurements (clock gating is modeled functionally, not power values)

#### RTL/Gate-Level Implementation Details
- Sparse FSM encoding implementation (FSM behavior is modeled, not encoding)
- Multibit signal encoding implementation details (functional validation modeled, not bit patterns)
- Clock gating primitive implementation (prim_clock_gating specifics)
- Bus integrity countermeasure circuit implementation
- Internal datapath width optimization
- Register file implementation structure
- SHA-2 accelerator internal architecture (hash function is modeled, not circuit)

#### Pin-Level Timing
- Setup and hold time requirements
- Clock-to-output delays
- Signal propagation delays
- Bus turnaround timing
- TileLink protocol physical layer timing parameters
- Signal skew and jitter specifications

#### Manufacturing and Process Parameters
- Process corner variations
- Temperature dependencies
- Voltage scaling effects
- Aging and reliability characteristics
- Silicon-specific optimizations

#### Test and Debug Infrastructure
- Design-for-Test (DFT) features
- Scan chain implementation
- JTAG/Debug access port details
- Manufacturing test modes
- BIST (Built-In Self-Test) mechanisms

#### Verification-Specific Features
- UVM testbench architecture details
- Coverage model implementation
- Assertion checking implementation (SVA binding)
- Scoreboard prediction algorithms
- Constrained random test sequences
- Formal verification properties

#### Unimplemented/Future Extensions
- Future inline encryption support (mentioned as extension, not current feature)
- Shared cryptographic module interface (mentioned as future option)
- Multi-channel DMA operation (explicitly single-channel only)
- Burst transfer optimization (currently single read/write at a time)
- DMA priority levels between channels (single-channel design)

#### SoC Integration Specifics
- RACL (Resource Access Control List) role assignment details (functional access control modeled)
- Mailbox interface DOE protocol specifics (external to DMA core)
- PLIC (Platform-Level Interrupt Controller) internal operation
- Clock domain crossing implementation details (functional crossing modeled)
- Reset domain crossing circuit implementation

#### Register Implementation Details
- Register reset value bit patterns (functional reset state modeled)
- Reserved register field implementation
- Register access protection circuit details (functional lock modeled)
- Read-modify-write hazard handling circuits

#### Physical Integration
- Floorplan and placement constraints
- Routing congestion considerations
- Power grid implementation
- Clock tree synthesis details

As the model is not timing accurate hence it is not suitable for performance measurements.

---

## 2. Functional Description

The DMA Controller is a single-channel bus master peripheral designed for autonomous data movement between memory regions and peripheral devices. It supports three distinct bus interfaces (OT Private, CTN, System) and includes inline SHA-2 hashing capability for data integrity verification during transfers.

### 2.1 DMA Config. Parameters

The following build-time configuration parameters define the static hardware structure of the DMA SystemC TLM model:

| Parameter Name | Type | Default/Example | Description |
|---|---|---|---|
| **NUM_CHANNELS** | uint8_t | 1 | The DMA is explicitly a single-channel device that processes one transaction at a time. This is a fundamental architectural constraint that affects FSM structure, arbitration logic, and resource allocation. |
| **NUM_BUS_INTERFACES** | uint8_t | 3 | Defines the number of bus master interfaces (OT Private, CTN, System). This is a fixed architectural feature that determines the number of TLM initiator sockets required. |
| **OT_ADDR_WIDTH** | uint8_t | 32 | Address width for OpenTitan internal bus interface (TL-UL). Fixed at 32 bits for OT internal address space. |
| **CTN_ADDR_WIDTH** | uint8_t | 32 or 64 | Address width for Control Network (CTN) bus interface. Can be configured as 32-bit or 64-bit depending on SoC integration. |
| **SYS_ADDR_WIDTH** | uint8_t | 64 | Address width for System bus interface. Fixed at 64 bits to support full system address space. |
| **BUS_DATA_WIDTH** | uint8_t | 32 | Data width of TL-UL bus interfaces (OT and CTN). Fixed at 32 bits for TileLink Uncached Lightweight protocol. |
| **SYS_BUS_DATA_WIDTH** | uint8_t | 64 | Data width of System bus interface. Fixed at 64 bits for custom system bus protocol. |
| **TRANSFER_WIDTH_OPTIONS** | uint8_t[3] | {1, 2, 4} | Supported transaction widths in bytes (1-byte, 2-byte, 4-byte). These are fixed hardware capabilities for sub-word extraction/replication logic. |
| **INLINE_HASH_ENABLE** | bool | true | Inline SHA-2 hashing feature presence. Determines whether hash computation hardware is instantiated. |
| **NUM_HASH_ALGORITHMS** | uint8_t | 3 | Number of supported hash algorithms (SHA-256, SHA-384, SHA-512). |
| **NUM_HANDSHAKE_INTR** | uint8_t | 11 | Number of hardware handshake interrupt inputs from low-speed I/O peripherals (lsio_trigger[10:0]). |
| **MAX_OUTSTANDING_TXNS** | uint8_t | 1 | Maximum number of outstanding bus transactions. Fixed at 1 (single outstanding transaction, no pipelining). |

#### Parameter Classification Rationale

**Included Parameters** represent:
1. **Fixed Hardware Structure**: Number of channels, bus interfaces, register arrays
2. **Static Address/Data Widths**: Address space sizes and data path widths that vary by interface but are fixed at synthesis
3. **Hardware Feature Presence**: Inline hashing support, number of hash algorithms
4. **Resource Limits**: Outstanding transaction depth, handshake interrupt count

**Excluded Items** - The following are NOT configuration parameters because they are runtime-programmable:
- Transfer sizes (TOTAL_DATA_SIZE, CHUNK_DATA_SIZE) - software configurable per transfer
- Addressing modes (increment, fixed, wrap) - selected via SRC_CONFIG/DST_CONFIG registers
- Address ranges (ENABLED_MEMORY_RANGE_BASE/LIMIT) - firmware-configurable at boot
- Interrupt enables - controlled via HANDSHAKE_INTR_ENABLE register
- Hash algorithm selection - chosen via CONTROL.opcode field per transfer
- Transfer width selection - programmed via TRANSFER_WIDTH register

#### Special Considerations

**CTN_ADDR_WIDTH Configurability**: The Control Network (CTN) interface address width is explicitly documented as configurable between 32-bit and 64-bit based on SoC integration requirements. This is the only interface with width variability and must be a compile-time parameter.

**Hardware Handshake Interrupt Count**: The NUM_HANDSHAKE_INTR parameter is derived from register bit field widths (11 bits in HANDSHAKE_INTR_ENABLE, CLEAR_INTR_SRC, CLEAR_INTR_BUS) and the array size of INTR_SRC_ADDR registers (0-10 = 11 entries). While documentation mentions "2 or 3" as typical, the hardware supports up to 11.

**Outstanding Transactions**: The architectural decision to support only single outstanding transactions affects TLM modeling complexity and must be reflected in the model's resource allocation and transaction pipelining behavior.

### 2.2 Port interfaces

The DMA controller is a **Bus Master with Configuration Slave** peripheral that:
- Acts as TL-UL/custom protocol master on three bus interfaces for autonomous data transfers
- Provides TL-UL slave interface for configuration register access
- Generates status interrupts to notify completion and error conditions
- Receives hardware handshake interrupt inputs from low-speed I/O peripherals

#### Port Interface Summary

| Interface Category | Port Name | Port Type | Description |
|---|---|---|---|
| **Register Bus** | reg_target_socket | tlm_target_socket<32> | TL-UL slave for configuration/control register access |
| **Bus Master - OT Private** | ot_initiator_socket | tlm_initiator_socket<32> | TL-UL master for OpenTitan internal memory access |
| **Bus Master - CTN** | ctn_initiator_socket | tlm_initiator_socket<32> or tlm_initiator_socket<64> | TL-UL master for SoC Control Network (configurable 32/64-bit) |
| **Bus Master - System** | sys_initiator_socket | tlm_initiator_socket<64> | Custom 64-bit system bus master for SoC memory |
| **Interrupt Outputs** | dma_done_intr | sc_out<bool> | Transfer completion interrupt |
| **Interrupt Outputs** | dma_chunk_done_intr | sc_out<bool> | Chunk completion interrupt (memory-to-memory mode) |
| **Interrupt Outputs** | dma_error_intr | sc_out<bool> | Error condition interrupt |
| **Hardware Handshake Inputs** | lsio_trigger[10:0] | sc_in<bool>[11] | Interrupt inputs from low-speed I/O peripherals (I2C, UART, SPI) |
| **Clock & Reset** | clk_i | sc_in<sc_time> | Abstract clock frequency input |
| **Clock & Reset** | rst_ni | sc_in<bool> | Active-low asynchronous reset input |

#### Detailed Port Specifications

##### Register Bus Interface

**reg_target_socket**
- **Type:** tlm_target_socket<32>
- **Protocol:** TL-UL (TileLink Uncached Lightweight)
- **Direction:** Target (Slave)
- **Address Width:** 32 bits
- **Data Width:** 32 bits
- **Purpose:** Configuration and control register access from system processor (Ibex)
- **Behavior:**
  - Provides access to DMA configuration registers (source/destination addresses, transfer sizes, control, status)
  - Supports standard TLM-2.0 blocking transport interface (b_transport)
  - Register space includes: INTR_STATE, INTR_ENABLE, CONTROL, STATUS, SRC_ADDR, DST_ADDR, ADDR_SPACE_ID, TRANSFER_WIDTH, etc.
  - Implements register locking during active transfers (CFG_REGWEN behavior)
  - Enforces multibit-encoded security fields (RANGE_REGWEN)
- **Transaction Types:**
  - TLM_READ_COMMAND: Register read operations
  - TLM_WRITE_COMMAND: Register write operations (configuration, control, interrupt clearing)
- **Timing:** Abstract timing with annotated delays for register access latency

##### Bus Master Interfaces

**ot_initiator_socket**
- **Type:** tlm_initiator_socket<32>
- **Protocol:** TL-UL (TileLink Uncached Lightweight)
- **Direction:** Initiator (Master)
- **Address Width:** 32 bits
- **Data Width:** 32 bits
- **Purpose:** Autonomous read/write access to OpenTitan internal (RoT-private) memory
- **Behavior:**
  - Initiates TL-UL read/write transactions for DMA data transfers to/from OT internal memory
  - Single outstanding transaction (no pipelining)
  - Supports sub-word transfers (1-byte, 2-byte, 4-byte) with byte-enable strobing
  - Address range restricted to 32-bit space (upper 32 bits of 64-bit address must be zero)
  - Enforces hardware security checks against DMA-enabled memory range registers
- **Transaction Types:**
  - TLM_READ_COMMAND: Read data from OT memory
  - TLM_WRITE_COMMAND: Write data to OT memory
  - Byte enable masks for sub-word transfers
- **Address Space:** OpenTitan internal 32-bit address space (ASID = OT_ADDR = 0x7)
- **Timing:** Annotated with abstract bus cycle delays

**ctn_initiator_socket**
- **Type:** tlm_initiator_socket<32> or tlm_initiator_socket<64> (configurable)
- **Protocol:** TL-UL (TileLink Uncached Lightweight)
- **Direction:** Initiator (Master)
- **Address Width:** 32 or 64 bits (SoC-dependent configuration parameter)
- **Data Width:** 32 bits
- **Purpose:** Autonomous read/write access to SoC Control Network (CTN)
- **Behavior:**
  - Initiates TL-UL transactions for data transfers to/from SoC control register bus
  - Single outstanding transaction
  - Supports sub-word transfers with byte-enable strobing
  - Address width configurable at compile-time (32-bit or 64-bit based on SoC integration)
  - If 32-bit mode, upper 32 bits of address registers must be zero (enforced by hardware checks)
- **Transaction Types:**
  - TLM_READ_COMMAND: Read from CTN address space
  - TLM_WRITE_COMMAND: Write to CTN address space
- **Address Space:** SoC control register bus (ASID = SOC_ADDR = 0xA)
- **Timing:** Annotated with abstract bus cycle delays

**sys_initiator_socket**
- **Type:** tlm_initiator_socket<64>
- **Protocol:** Custom 64-bit System Bus (non-TL-UL)
- **Direction:** Initiator (Master)
- **Address Width:** 64 bits
- **Data Width:** 64 bits
- **Purpose:** High-performance read/write access to SoC system memory (full 64-bit address space)
- **Behavior:**
  - Initiates read/write transactions using custom system bus protocol
  - Separated read and write channels (unlike TL-UL)
  - Single outstanding transaction (no burst mode)
  - Full 64-bit address space support for system memory
  - RACL (Resource Access Control) role signaling for security enforcement
  - Supports error responses (bus errors propagated to ERROR_CODE register)
- **Transaction Types:**
  - TLM_READ_COMMAND: Read from system memory (64-bit data)
  - TLM_WRITE_COMMAND: Write to system memory (64-bit data)
  - Byte enables for sub-word transfers
  - Metadata/RACL role information
- **Address Space:** SoC 64-bit system address space (ASID = SYS_ADDR = 0x9)
- **Timing:** Annotated with abstract bus cycle and memory access delays
- **Custom Attributes:**
  - vld_vec: Request valid signal
  - opcode_vec: Read/Write opcode
  - iova_vec: 64-bit address (IO Virtual Address)
  - racl_vec: RACL role identifier
  - write_data, write_be, read_be: Data and byte enables
  - grant_vec: Response accept acknowledgment
  - error_vld, error_vec: Error signaling

##### Interrupt Output Interfaces

**dma_done_intr**
- **Type:** sc_out<bool>
- **Direction:** Output
- **Purpose:** Transfer completion notification
- **Behavior:**
  - Asserted (level-high) when entire DMA transfer completes (TOTAL_DATA_SIZE bytes transferred)
  - Status-type interrupt (remains asserted until cleared by software write to STATUS.done)
  - Reflects INTR_STATE.dma_done register bit
  - Gated by INTR_ENABLE.dma_done enable bit
- **Timing:** Abstract event notification (no cycle-accurate timing required)

**dma_chunk_done_intr**
- **Type:** sc_out<bool>
- **Direction:** Output
- **Purpose:** Chunk completion notification (memory-to-memory transfers only)
- **Behavior:**
  - Asserted (level-high) after each chunk transfer completes (CHUNK_DATA_SIZE bytes)
  - Only active for multi-chunk memory-to-memory transfers (not in hardware handshake mode)
  - Status-type interrupt (cleared by software write to STATUS.chunk_done or automatically on next chunk start)
  - Reflects INTR_STATE.dma_chunk_done register bit
  - Gated by INTR_ENABLE.dma_chunk_done enable bit
- **Timing:** Abstract event notification

**dma_error_intr**
- **Type:** sc_out<bool>
- **Direction:** Output
- **Purpose:** Error condition notification
- **Behavior:**
  - Asserted (level-high) when DMA error occurs (configuration error, bus error, security violation)
  - Status-type interrupt (cleared by software write to STATUS.error)
  - Error details available in ERROR_CODE register (source address error, destination address error, ASID validation error, bus error, etc.)
  - Reflects INTR_STATE.dma_error register bit
  - Gated by INTR_ENABLE.dma_error enable bit
- **Timing:** Abstract event notification

##### Hardware Handshake Interrupt Inputs

**lsio_trigger[10:0]**
- **Type:** sc_in<bool>[11] (array of 11 interrupt inputs)
- **Direction:** Input
- **Purpose:** Hardware handshake triggers from low-speed I/O peripherals (I2C, UART, SPI Device, SPI Host)
- **Behavior:**
  - Level-sensitive interrupt inputs (active-high)
  - Each bit corresponds to a specific peripheral FIFO status (threshold reached)
  - Enabled/disabled via HANDSHAKE_INTR_ENABLE register (11-bit mask)
  - When enabled and asserted, DMA initiates chunk transfer without CPU intervention
  - Peripheral automatically de-asserts signal after FIFO condition resolved (no explicit clearing needed in most cases)
  - Optional interrupt source clearing via configurable write transaction (CLEAR_INTR_SRC, INTR_SRC_ADDR, INTR_SRC_WR_VAL registers)
  - Supports autonomous FIFO servicing for:
    - I2C RX/TX FIFOs
    - UART RX/TX FIFOs
    - SPI Device RX/TX FIFOs
    - SPI Host RX/TX FIFOs
- **Configuration:**
  - HANDSHAKE_INTR_ENABLE[10:0]: Enable mask for each interrupt line
  - CLEAR_INTR_SRC[10:0]: Interrupt sources requiring software-initiated clearing
  - CLEAR_INTR_BUS[10:0]: Bus selection for interrupt clearing (OT internal vs. CTN/System)
  - INTR_SRC_ADDR_0 through INTR_SRC_ADDR_10: Clearing write target addresses
  - INTR_SRC_WR_VAL_0 through INTR_SRC_WR_VAL_10: Clearing write data values
- **Timing:** Abstract event-driven signaling (level-sensitive)

##### Clock and Reset Interfaces

**clk_i**
- **Type:** sc_in<sc_time>
- **Direction:** Input
- **Purpose:** Abstract clock frequency for functional timing
- **Behavior:**
  - Provides clock period for abstract timing annotations (not cycle-accurate simulation)
  - Used to calculate transfer durations and timing delays
  - Functional clock gating modeled as automatic power state (not electrical gating)
  - Clock gate enabled during active transfers, disabled when idle (software-invisible)
- **Notes:**
  - TLM model abstracts away clock edges and phase relationships
  - Single synchronous clock domain assumed
  - Clock domain crossings outside DMA scope (handled by SoC integration)

**rst_ni**
- **Type:** sc_in<bool>
- **Direction:** Input
- **Purpose:** Asynchronous active-low reset
- **Behavior:**
  - Resets all internal state and configuration registers to default values
  - Asynchronous assertion, synchronous de-assertion (abstract in TLM)
  - Clears ongoing transfers and returns to idle state
  - Unlocks RANGE_REGWEN and CFG_REGWEN registers
  - De-asserts all interrupt outputs
- **Notes:**
  - Reset timing abstracted (no setup/hold requirements)
  - Reset domain crossings outside DMA scope

#### Address Space Identifier (ASID) Mapping

| ASID Value | Name | Description | Socket | Usable in SEP? |
|---|---|---|---|---|
| 0x7 | OT_ADDR | OpenTitan 32-bit internal bus | ot_initiator_socket | Yes |
| 0xA | SOC_ADDR | SoC control register bus (32-bit or 64-bit) | ctn_initiator_socket | No — tied off |
| 0x9 | SYS_ADDR | SoC system address bus (64-bit) | sys_initiator_socket | No — tied off |

Source and destination ASID configured via ADDR_SPACE_ID register (src_asid[2:0], dst_asid[2:0]).

**Only the OT leg is connected in SEP.** `sep_dma_wrap.sv` grounds `sys_i` and stubs the CTN
interface with `a_ready` high and `d_valid` low, so a request on either ASID is accepted and
no response ever arrives — the DMA stalls indefinitely. The SEP platform binds both sockets
to `dead_manager_port_stub`, which returns an error instead. This is a **deliberate
divergence**: `b_transport` has no way to express "never responds" without hanging the
SystemC kernel, so the transfer fails visibly rather than by deadlock. Either way the
transfer does not succeed, which is the property firmware depends on.

The standalone unit testbench binds ordinary memories to all three sockets, so FUNC-009's
CTN and SYS cases still exercise the datapath. That coverage is about the model's addressing
and width handling, not about what SEP wires up.

#### Address remapping on the OT path

`sep_dma_wrap.sv` places an `axi_window_remap` on the DMA's OT initiator: an address in
`[SEP_LOCAL_BASE_ADDR, SEP_LOCAL_BASE_ADDR + 0x3000_0000)` is rewritten to
`addr - SEP_LOCAL_BASE_ADDR + 0x1000_0000`. The platform reproduces this with
`dma_alias_remap_adapter` spliced into the same path, so the DMA's view of the address map
matches the CPU's. The model itself does not remap — this belongs to the wrapper, and a
standalone instantiation sees unremapped addresses.

#### Transaction-Level Modeling Notes

**Transfer Behavior Abstractions**

1. **Single Outstanding Transaction:** DMA issues one read or write at a time (no pipelining). Model enforces sequential transaction completion before issuing next access.

2. **Sub-word Transfers:** TLM byte-enable attributes model sub-word extraction/replication:
   - Source reads: Extract correct byte lane based on address LSBs
   - Destination writes: Replicate sub-word across data bus width, use byte-enables to select lanes

3. **Chunked Transfers:** Model processes CHUNK_DATA_SIZE bytes, then waits for:
   - Memory-to-memory: Optional chunk_done interrupt acknowledgment
   - Hardware handshake: Next lsio_trigger assertion

4. **Inline Hashing:** SHA-2 hashing (SHA-256/384/512) modeled as:
   - Data transformation function applied during transfer
   - Digest written to SHA2_DIGEST_0 through SHA2_DIGEST_15 registers
   - Controlled by CONTROL.opcode field (COPY vs. SHA256/384/512)
   - initial_transfer bit controls hash state initialization

5. **Security Enforcement:** Model validates:
   - Address range checks against ENABLED_MEMORY_RANGE_BASE/LIMIT
   - ASID-based access control matrix (OT Private/DMA-enabled/SoC memory restrictions)
   - Multibit-encoded security signals (abstract as functional checks, not bit patterns)

6. **Error Handling:** Model generates dma_error_intr and populates ERROR_CODE register for:
   - Configuration errors (invalid transfer width, zero size)
   - Address errors (out of DMA-enabled range, ASID violations)
   - Bus errors (target device error responses)

**Exclusions (Not Modeled)**
- RTL clock gating circuits (prim_clock_gating primitives)
- Pin-level TL-UL protocol timing (setup/hold, turnaround)
- Sparse FSM encoding details
- Multibit signal bit patterns (modeled as functional validation)
- Bus integrity countermeasure circuits (end-to-end integrity abstract)
- Physical reset characteristics

### 2.3 Memory-Mapped Registers

The DMA Controller module's registers are accessed through the peripheral bus at the module's base address. All configuration, status, and data transfer is handled via these registers.

#### Register Map

| Register Name | Offset | Size (bits) | Access | Reset Value | Description/Notes |
|---|---|---|---|---|---|
| **Interrupt Control Registers** |
| INTR_STATE | 0x00 | 32 | RO | 0x00000000 | Interrupt state register. Bits [2:0]: dma_error, dma_chunk_done, dma_done. Reserved bits [31:3] are read-only zero. |
| INTR_ENABLE | 0x04 | 32 | RW | 0x00000000 | Interrupt enable register. Bits [2:0]: dma_error, dma_chunk_done, dma_done enable bits. Reserved bits [31:3] are read/write zero. |
| INTR_TEST | 0x08 | 32 | WO | 0x00000000 | Interrupt test register for forcing interrupt conditions. Bits [2:0]: write 1 to force corresponding interrupt. Reserved bits [31:3] are write-only zero. |
| **Alert Control Register** |
| ALERT_TEST | 0x0C | 32 | WO | 0x00000000 | Alert test register. Bit [0]: fatal_fault trigger. Reserved bits [31:1] are write-only zero. |
| **Source Address Configuration** |
| SRC_ADDR_LO | 0x10 | 32 | RW | 0x00000000 | Lower 32 bits of source address. Must be aligned to transfer width. Locked by CFG_REGWEN during operation. |
| SRC_ADDR_HI | 0x14 | 32 | RW | 0x00000000 | Upper 32 bits of source address. Must be aligned to transfer width. Locked by CFG_REGWEN during operation. |
| **Destination Address Configuration** |
| DST_ADDR_LO | 0x18 | 32 | RW | 0x00000000 | Lower 32 bits of destination address. Must be aligned to transfer width. Locked by CFG_REGWEN during operation. |
| DST_ADDR_HI | 0x1C | 32 | RW | 0x00000000 | Upper 32 bits of destination address. Must be aligned to transfer width. Locked by CFG_REGWEN during operation. |
| **Address Space Configuration** |
| ADDR_SPACE_ID | 0x20 | 32 | RW | 0x00000077 | Address space identifiers. Bits [3:0]: src_asid (default 0x7=OT_ADDR), Bits [7:4]: dst_asid (default 0x7=OT_ADDR). Valid values: 0x7=OT_ADDR (32-bit OT internal), 0xA=SOC_ADDR (32/64-bit CTN), 0x9=SYS_ADDR (64-bit system). Reserved bits [31:8]. Locked by CFG_REGWEN. |
| **Memory Range Security Registers** |
| ENABLED_MEMORY_RANGE_BASE | 0x24 | 32 | RW | 0x00000000 | Base address for DMA-enabled memory range within OT internal address space. Locked by RANGE_REGWEN. |
| ENABLED_MEMORY_RANGE_LIMIT | 0x28 | 32 | RW | 0x00000000 | Limit address (inclusive) for DMA-enabled memory range within OT internal address space. Locked by RANGE_REGWEN. |
| RANGE_VALID | 0x2C | 32 | RW | 0x00000000 | Range validity indicator. Bit [0]: once set, indicates base/limit are valid. Reserved bits [31:1]. Locked by RANGE_REGWEN. |
| RANGE_REGWEN | 0x30 | 32 | RW0C | 0x00000006 | Range register write-enable lock, MuBi4-encoded: `0x6` = MuBi4True = unlocked, `0x9` = MuBi4False = locked. Writing **any** value other than `0x6` locks the range registers until reset, and the register then reads back `0x9`. Reserved bits [31:4]. |
| **Configuration Lock Register** |
| CFG_REGWEN | 0x34 | 32 | RO | 0x00000006 | Configuration register lock status (hardware-managed), MuBi4-encoded: `0x6` = unlocked/idle, `0x9` = locked/busy. Reads straight off the busy bit. Reserved bits [31:4]. |
| **Transfer Size Configuration** |
| TOTAL_DATA_SIZE | 0x38 | 32 | RW | 0x00000000 | Total transfer size in bytes (minimum 1 byte). Complete transfer may consist of multiple chunks. Locked by CFG_REGWEN during operation. |
| CHUNK_DATA_SIZE | 0x3C | 32 | RW | 0x00000000 | Chunk size in bytes for hardware handshake or chunked transfers (minimum 1 byte). For single memory transfer, set equal to TOTAL_DATA_SIZE. Locked by CFG_REGWEN. |
| **Transfer Configuration** |
| TRANSFER_WIDTH | 0x40 | 32 | RW | 0x00000002 | Transaction width. Bits [1:0]: 0x0=ONE_BYTE (1 byte), 0x1=TWO_BYTE (2 bytes), 0x2=FOUR_BYTE (4 bytes, default). Value 0x3 is invalid and causes error. Reserved bits [31:2]. Locked by CFG_REGWEN. |
| **Control Register** |
| CONTROL | 0x44 | 32 | Mixed | 0x00000000 | DMA control register. Bit [31]: go (RW, start transfer), Bit [27]: abort (WO, abort transfer), Bit [8]: initial_transfer (RW, marks first transfer for hash init), Bit [5]: digest_swap (RW, byte-swap digest output), Bit [4]: hardware_handshake_enable (RW, enable HW handshake mode), Bits [3:0]: opcode (RW, 0x0=COPY, 0x1=SHA256, 0x2=SHA384, 0x3=SHA512). Reserved bits [30:28, 26:9, 7:6]. |
| **Addressing Mode Configuration** |
| SRC_CONFIG | 0x48 | 32 | RW | 0x00000000 | Source addressing mode. Bit [0]: increment (0=fixed address, 1=auto-increment by transfer_width), Bit [1]: wrap (0=contiguous chunks, 1=wrap to start address per chunk). Reserved bits [31:2]. Locked by CFG_REGWEN. |
| DST_CONFIG | 0x4C | 32 | RW | 0x00000000 | Destination addressing mode. Bit [0]: increment (0=fixed address, 1=auto-increment by transfer_width), Bit [1]: wrap (0=contiguous chunks, 1=wrap to start address per chunk). Reserved bits [31:2]. Locked by CFG_REGWEN. |
| **Status Register** |
| STATUS | 0x50 | 32 | Mixed | 0x00000000 | DMA status. Bit [0]: busy (RO, 1=active), Bit [1]: done (RW1C, transfer complete, auto-cleared on new transfer), Bit [2]: aborted (RW1C, abort complete), Bit [3]: error (RW1C, error occurred), Bit [4]: sha2_digest_valid (RO, digest valid), Bit [5]: chunk_done (RW1C, chunk complete, auto-cleared on next chunk or by write). Reserved bits [31:6]. |
| **Error Code Register** |
| ERROR_CODE | 0x54 | 32 | RO | 0x00000000 | Error source indicators (cleared by writing STATUS.error). Bit [0]: src_addr_error, Bit [1]: dst_addr_error, Bit [2]: opcode_error, Bit [3]: size_error (invalid width, zero size, or hash without 32-bit width), Bit [4]: bus_error, Bit [5]: base_limit_error, Bit [6]: range_valid_error, Bit [7]: asid_error. Reserved bits [31:8]. |
| **SHA-2 Digest Registers** |
| SHA2_DIGEST_0 | 0x58 | 32 | RO | 0x00000000 | SHA-2 digest word 0 (bytes 0-3 of digest). Used by all SHA-2 modes (SHA-256/384/512). |
| SHA2_DIGEST_1 | 0x5C | 32 | RO | 0x00000000 | SHA-2 digest word 1 (bytes 4-7 of digest). Used by all SHA-2 modes. |
| SHA2_DIGEST_2 | 0x60 | 32 | RO | 0x00000000 | SHA-2 digest word 2 (bytes 8-11 of digest). Used by all SHA-2 modes. |
| SHA2_DIGEST_3 | 0x64 | 32 | RO | 0x00000000 | SHA-2 digest word 3 (bytes 12-15 of digest). Used by all SHA-2 modes. |
| SHA2_DIGEST_4 | 0x68 | 32 | RO | 0x00000000 | SHA-2 digest word 4 (bytes 16-19 of digest). Used by all SHA-2 modes. |
| SHA2_DIGEST_5 | 0x6C | 32 | RO | 0x00000000 | SHA-2 digest word 5 (bytes 20-23 of digest). Used by all SHA-2 modes. |
| SHA2_DIGEST_6 | 0x70 | 32 | RO | 0x00000000 | SHA-2 digest word 6 (bytes 24-27 of digest). Used by all SHA-2 modes. |
| SHA2_DIGEST_7 | 0x74 | 32 | RO | 0x00000000 | SHA-2 digest word 7 (bytes 28-31 of digest). Used by SHA-256 (last word), SHA-384/512 (continued). |
| SHA2_DIGEST_8 | 0x78 | 32 | RO | 0x00000000 | SHA-2 digest word 8 (bytes 32-35 of digest). Used by SHA-384/512 only. |
| SHA2_DIGEST_9 | 0x7C | 32 | RO | 0x00000000 | SHA-2 digest word 9 (bytes 36-39 of digest). Used by SHA-384/512 only. |
| SHA2_DIGEST_10 | 0x80 | 32 | RO | 0x00000000 | SHA-2 digest word 10 (bytes 40-43 of digest). Used by SHA-384/512 only. |
| SHA2_DIGEST_11 | 0x84 | 32 | RO | 0x00000000 | SHA-2 digest word 11 (bytes 44-47 of digest). Used by SHA-384 (last word), SHA-512 (continued). |
| SHA2_DIGEST_12 | 0x88 | 32 | RO | 0x00000000 | SHA-2 digest word 12 (bytes 48-51 of digest). Used by SHA-512 only. |
| SHA2_DIGEST_13 | 0x8C | 32 | RO | 0x00000000 | SHA-2 digest word 13 (bytes 52-55 of digest). Used by SHA-512 only. |
| SHA2_DIGEST_14 | 0x90 | 32 | RO | 0x00000000 | SHA-2 digest word 14 (bytes 56-59 of digest). Used by SHA-512 only. |
| SHA2_DIGEST_15 | 0x94 | 32 | RO | 0x00000000 | SHA-2 digest word 15 (bytes 60-63 of digest). Used by SHA-512 (last word). |
| **Hardware Handshake Configuration** |
| HANDSHAKE_INTR_ENABLE | 0x98 | 32 | RW | 0x00000000 | Hardware handshake interrupt enable mask. Bits [10:0]: enable bits for lsio_trigger[10:0] inputs. Reserved bits [31:11]. Locked by CFG_REGWEN. |
| CLEAR_INTR_SRC | 0x9C | 32 | RW | 0x00000000 | Interrupt source clearing enable. Bits [10:0]: enable automatic interrupt clearing for each lsio_trigger source. Reserved bits [31:11]. Locked by CFG_REGWEN. |
| CLEAR_INTR_BUS | 0xA0 | 32 | RW | 0x00000000 | Bus selection for interrupt clearing. Bits [10:0]: 0=CTN/System fabric, 1=OT-internal crossbar. Reserved bits [31:11]. Locked by CFG_REGWEN. |
| INTR_SRC_ADDR_0 | 0xA4 | 32 | RW | 0x00000000 | Interrupt source 0 clearing write address. Locked by CFG_REGWEN. |
| INTR_SRC_ADDR_1 | 0xA8 | 32 | RW | 0x00000000 | Interrupt source 1 clearing write address. Locked by CFG_REGWEN. |
| INTR_SRC_ADDR_2 | 0xAC | 32 | RW | 0x00000000 | Interrupt source 2 clearing write address. Locked by CFG_REGWEN. |
| INTR_SRC_ADDR_3 | 0xB0 | 32 | RW | 0x00000000 | Interrupt source 3 clearing write address. Locked by CFG_REGWEN. |
| INTR_SRC_ADDR_4 | 0xB4 | 32 | RW | 0x00000000 | Interrupt source 4 clearing write address. Locked by CFG_REGWEN. |
| INTR_SRC_ADDR_5 | 0xB8 | 32 | RW | 0x00000000 | Interrupt source 5 clearing write address. Locked by CFG_REGWEN. |
| INTR_SRC_ADDR_6 | 0xBC | 32 | RW | 0x00000000 | Interrupt source 6 clearing write address. Locked by CFG_REGWEN. |
| INTR_SRC_ADDR_7 | 0xC0 | 32 | RW | 0x00000000 | Interrupt source 7 clearing write address. Locked by CFG_REGWEN. |
| INTR_SRC_ADDR_8 | 0xC4 | 32 | RW | 0x00000000 | Interrupt source 8 clearing write address. Locked by CFG_REGWEN. |
| INTR_SRC_ADDR_9 | 0xC8 | 32 | RW | 0x00000000 | Interrupt source 9 clearing write address. Locked by CFG_REGWEN. |
| INTR_SRC_ADDR_10 | 0xCC | 32 | RW | 0x00000000 | Interrupt source 10 clearing write address. Locked by CFG_REGWEN. |
| INTR_SRC_WR_VAL_0 | 0xD0 | 32 | RW | 0x00000000 | Interrupt source 0 clearing write data value. Locked by CFG_REGWEN. |
| INTR_SRC_WR_VAL_1 | 0xD4 | 32 | RW | 0x00000000 | Interrupt source 1 clearing write data value. Locked by CFG_REGWEN. |
| INTR_SRC_WR_VAL_2 | 0xD8 | 32 | RW | 0x00000000 | Interrupt source 2 clearing write data value. Locked by CFG_REGWEN. |
| INTR_SRC_WR_VAL_3 | 0xDC | 32 | RW | 0x00000000 | Interrupt source 3 clearing write data value. Locked by CFG_REGWEN. |
| INTR_SRC_WR_VAL_4 | 0xE0 | 32 | RW | 0x00000000 | Interrupt source 4 clearing write data value. Locked by CFG_REGWEN. |
| INTR_SRC_WR_VAL_5 | 0xE4 | 32 | RW | 0x00000000 | Interrupt source 5 clearing write data value. Locked by CFG_REGWEN. |
| INTR_SRC_WR_VAL_6 | 0xE8 | 32 | RW | 0x00000000 | Interrupt source 6 clearing write data value. Locked by CFG_REGWEN. |
| INTR_SRC_WR_VAL_7 | 0xEC | 32 | RW | 0x00000000 | Interrupt source 7 clearing write data value. Locked by CFG_REGWEN. |
| INTR_SRC_WR_VAL_8 | 0xF0 | 32 | RW | 0x00000000 | Interrupt source 8 clearing write data value. Locked by CFG_REGWEN. |
| INTR_SRC_WR_VAL_9 | 0xF4 | 32 | RW | 0x00000000 | Interrupt source 9 clearing write data value. Locked by CFG_REGWEN. |
| INTR_SRC_WR_VAL_10 | 0xF8 | 32 | RW | 0x00000000 | Interrupt source 10 clearing write data value. Locked by CFG_REGWEN. |

#### Register Access Legend

- **RO** = Read Only
- **RW** = Read/Write
- **WO** = Write Only
- **RW1C** = Write-1-to-Clear (write 1 to clear the bit, writing 0 has no effect)
- **RW0C** = Write-0-to-Clear (write 0 to lock/clear, writing 1 has no effect)
- **Mixed** = Register contains fields with different access types

#### Reserved Bit Behavior

Reserved bits in all registers are read-only and return zero when read. Writes to reserved bits are ignored (no side effects). Reserved address ranges are not decoded and will return bus error responses if accessed.

#### Register Locking Mechanisms

The DMA controller implements two distinct register locking mechanisms:

1. **RANGE_REGWEN** (Software Lock): Controls access to memory range security registers (ENABLED_MEMORY_RANGE_BASE, ENABLED_MEMORY_RANGE_LIMIT, RANGE_VALID). Firmware-controlled, and once locked it can only be unlocked by a full reset.

2. **CFG_REGWEN** (Hardware Lock): Automatically managed by hardware to lock transfer configuration registers during active DMA operations. This register is read-only for software and tracks the busy bit: locked (`0x9`) while the DMA is busy, unlocked (`0x6`) when idle. The CONTROL and STATUS registers remain accessible even when CFG_REGWEN indicates locked state.

Note: Comportable interrupt/alert configuration registers (INTR_STATE, INTR_ENABLE, INTR_TEST, ALERT_TEST) are NOT locked by CFG_REGWEN and remain modifiable during DMA operation.

#### Multi-Bit Encoded Security Values

The following registers use multi-bit encoding for enhanced security:

- **RANGE_REGWEN** [3:0]: `0x6` (kMultiBitBool4True) = Unlocked, `0x9` (kMultiBitBool4False) = Locked
- **CFG_REGWEN** [3:0]: `0x6` = Unlocked/Idle, `0x9` = Locked/Busy
- **ADDR_SPACE_ID** src_asid/dst_asid: Multi-bit encoded ASID values (0x7, 0x9, 0xA are valid; other values trigger asid_error)

**The locked value is `0x9`, not `0x0`.** This follows the MuBi4 write arbitration in
`prim_subreg_arb.sv`: `RANGE_REGWEN` stays `0x6` only if it currently holds `0x6` *and* the
incoming write is also `0x6`. Any other write — including `0x0`, and including a corrupted
value — settles the register on `0x9`. So the lock is "not MuBi4True" rather than "zero",
and the model's `is_range_locked()` tests exactly that. A test asserting `0x0` on a locked
REGWEN is testing the wrong encoding.

#### Memory Range Validation

`RANGE_VALID` is checked on **every** transfer, not only on transfers that cross the
enabled-range boundary. A transfer configured without it fails validation with
`ERROR_CODE.range_valid_error` and never starts, matching the `DmaAddrSetup` check in RTL.
This check lives in `validate_transfer_configuration()`; it is deliberately kept out of
`validate_security_policy()` so that range validity and access policy stay separate
concerns.

#### Address Register Writeback

The address CSRs are not a live view of the transfer's progress. They are written back only
for a port in **fixed** mode — `increment == 0 && wrap == 0` — and only at chunk end and at
completion, where `chunk_data_size` is added. In increment or wrap mode the CSRs keep their
programmed value for the whole transfer while the engine advances its internal address.

This mirrors the RTL's `update_src_addr_reg` / `update_dst_addr_reg` conditions exactly, and
it is the opposite of the intuitive expectation, so it is worth restating: **a test that
watches `SRC_ADDR_LO` climb during an incrementing transfer is asserting behaviour the
hardware does not have.**

A related convention: a fixed-address FIFO endpoint is configured with `SRC_CONFIG = 0x2`
(wrap set, increment clear), not `0x0`. This is what pins the address across a chunk, and it
is what the harness firmware uses for hardware-handshake sources.

#### Hardware Handshake Interrupt Clearing Mechanism

The DMA controller supports automatic interrupt acknowledgment for hardware handshake mode via configurable write transactions:

1. **HANDSHAKE_INTR_ENABLE** [10:0]: Enable mask for 11 lsio_trigger input lines
2. **CLEAR_INTR_SRC** [10:0]: Per-source enable for automatic clearing (when bit N is set, DMA performs clearing write for trigger N)
3. **CLEAR_INTR_BUS** [10:0]: Per-source bus selection (0=CTN/System fabric, 1=OT-internal crossbar)
4. **INTR_SRC_ADDR_N**: 32-bit address for clearing write transaction
5. **INTR_SRC_WR_VAL_N**: 32-bit data value for clearing write transaction

When an enabled lsio_trigger[N] asserts and CLEAR_INTR_SRC[N]=1, the DMA automatically issues a write to INTR_SRC_ADDR_N with data INTR_SRC_WR_VAL_N on the bus specified by CLEAR_INTR_BUS[N].

**If that clearing write takes a bus error, the transfer halts.** `ERROR_CODE.bus_error` is
set, `STATUS.error` is raised, `STATUS.busy` and `CONTROL.go` are cleared, and the error
interrupt fires. The DMA does not continue with the remaining chunks. This matches the RTL,
where `intr_clear_tlul_rsp_error` drives the FSM into `DmaError`.

#### Address Alignment Requirements

Source and destination addresses must be aligned to the configured TRANSFER_WIDTH:
- **ONE_BYTE** (0x0): No alignment requirement (byte-aligned)
- **TWO_BYTE** (0x1): 2-byte alignment (address[0] must be 0)
- **FOUR_BYTE** (0x2): 4-byte alignment (address[1:0] must be 0)

Misaligned addresses will trigger src_addr_error or dst_addr_error in the ERROR_CODE register.

#### SHA-2 Digest Output Mapping

Depending on the selected opcode in the CONTROL register, different numbers of digest registers contain valid data:

- **SHA-256** (opcode 0x1): SHA2_DIGEST_0 through SHA2_DIGEST_7 (256 bits = 32 bytes)
- **SHA-384** (opcode 0x2): SHA2_DIGEST_0 through SHA2_DIGEST_11 (384 bits = 48 bytes)
- **SHA-512** (opcode 0x3): SHA2_DIGEST_0 through SHA2_DIGEST_15 (512 bits = 64 bytes)

The digest_swap bit in CONTROL controls endianness conversion of each 32-bit digest word (does not affect register order). The sha2_digest_valid bit in STATUS indicates when digest registers contain a valid result.

---

## 3. Use model

The DMA will be modeled at LT abstraction level with timing annotation and temporal decoupling. The intended use case of this model is software development.

### 3.1 Callbacks on Memory-mapped registers/bit-fields

The following DMA controller registers require read/write callback functions due to functional side-effects, state changes, or conditional access dependencies. These callbacks implement transaction-level behavior beyond simple storage.

#### Registers Requiring Write Callbacks

| Callback Name | Type | Description |
|---|---|---|
| **handle_write_CONTROL** | Write | Triggers DMA operation when go bit is set. Aborts DMA operation when abort bit is set. Clears go bit automatically after normal operation completion (hardware handshake mode exception). Sets aborted status bit when abort completes. Initializes DMA engine and SHA-2 hash state when initial_transfer bit is set. |
| **handle_write_STATUS** | Write | Clears interrupt status bits when software writes 1 to RW1C fields: done (bit 1), aborted (bit 2), error (bit 3), chunk_done (bit 5). Also clears ERROR_CODE register content when error bit is cleared. De-asserts corresponding interrupt output signals. |
| **handle_write_INTR_TEST** | Write | Forces interrupt state bits in INTR_STATE register when written: dma_done (bit 0), dma_chunk_done (bit 1), dma_error (bit 2). Generates corresponding interrupt output signals for testing. |
| **handle_write_ALERT_TEST** | Write | Triggers fatal_fault alert when bit 0 is written to 1. Used for testing alert generation mechanism. |
| **handle_write_RANGE_REGWEN** | Write | Locks memory range configuration registers (ENABLED_MEMORY_RANGE_BASE, ENABLED_MEMORY_RANGE_LIMIT, RANGE_VALID) when written to 0x0 (kMultiBitBool4False). Once locked, these registers become read-only until next reset. Write-0-to-lock mechanism prevents further modification of security-critical memory range configuration. |

#### Registers Requiring Read Callbacks

| Callback Name | Type | Description |
|---|---|---|
| **handle_read_CFG_REGWEN** | Read | Returns hardware-managed lock status reflecting current DMA busy/idle state. Returns 0x0 (kMultiBitBool4False/locked) when DMA is busy executing a transfer. Returns 0x6 (kMultiBitBool4True/unlocked) when DMA is idle. This is a read-only register automatically updated by hardware state machine. |
| **handle_read_STATUS** | Read | Returns current DMA operational status including: busy bit (DMA actively executing transfer), done bit (transfer completion), aborted bit (abort operation completed), error bit (error condition occurred), sha2_digest_valid bit (digest registers contain valid hash), chunk_done bit (chunk transfer completed). Values reflect real-time hardware state. |
| **handle_read_ERROR_CODE** | Read | Returns error classification when STATUS.error is set. Reflects detected error conditions: src_addr_error, dst_addr_error, opcode_error, size_error, bus_error, base_limit_error, range_valid_error, asid_error. Cleared when STATUS.error is cleared via write callback. |
| **handle_read_SHA2_DIGEST_n** | Read | Returns hash digest output registers (SHA2_DIGEST_0 through SHA2_DIGEST_15). Valid digest data available when STATUS.sha2_digest_valid is set. Number of valid registers depends on hash algorithm: SHA-256 uses 0-7 (256 bits), SHA-384 uses 0-11 (384 bits), SHA-512 uses 0-15 (512 bits). Endianness controlled by CONTROL.digest_swap bit. |
| **handle_read_INTR_STATE** | Read | Returns current interrupt state reflecting pending interrupts: dma_done (bit 0), dma_chunk_done (bit 1), dma_error (bit 2). Cleared by writing 1 to corresponding bits in STATUS register. Read-only status register updated by hardware events. |

#### Registers Requiring Both Read and Write Callbacks

| Callback Name | Type | Description |
|---|---|---|
| **handle_write_SRC_ADDR_LO, handle_read_SRC_ADDR_LO** | Both | Write: Enforces transfer width alignment requirements and CFG_REGWEN lock protection. Read: May reflect updated address during active transfer if increment mode is enabled in SRC_CONFIG. Hardware automatically advances address by transfer_width after each read transaction. |
| **handle_write_SRC_ADDR_HI, handle_read_SRC_ADDR_HI** | Both | Write: Enforces upper 32-bit address constraints based on ADDR_SPACE_ID configuration (must be zero for OT_ADDR and CTN_ADDR). Protected by CFG_REGWEN lock. Read: May reflect updated address during active transfer. |
| **handle_write_DST_ADDR_LO, handle_read_DST_ADDR_LO** | Both | Write: Enforces transfer width alignment requirements and CFG_REGWEN lock protection. Read: May reflect updated address during active transfer if increment mode is enabled in DST_CONFIG. Hardware automatically advances address by transfer_width after each write transaction. |
| **handle_write_DST_ADDR_HI, handle_read_DST_ADDR_HI** | Both | Write: Enforces upper 32-bit address constraints based on ADDR_SPACE_ID configuration (must be zero for OT_ADDR and CTN_ADDR). Protected by CFG_REGWEN lock. Read: May reflect updated address during active transfer. |

#### Storage-Only Registers (No Callbacks Required)

The following registers are simple storage elements with no immediate functional side-effects:

- **INTR_ENABLE** - Configuration register for interrupt masking (no hardware action on write)
- **ADDR_SPACE_ID** - Static configuration for address space identifiers (locked by CFG_REGWEN)
- **ENABLED_MEMORY_RANGE_BASE** - Static memory range base address (locked by RANGE_REGWEN)
- **ENABLED_MEMORY_RANGE_LIMIT** - Static memory range limit address (locked by RANGE_REGWEN)
- **RANGE_VALID** - Static range validity indicator (locked by RANGE_REGWEN)
- **TOTAL_DATA_SIZE** - Transfer size configuration (locked by CFG_REGWEN)
- **CHUNK_DATA_SIZE** - Chunk size configuration (locked by CFG_REGWEN)
- **TRANSFER_WIDTH** - Transaction width configuration (locked by CFG_REGWEN)
- **SRC_CONFIG** - Source addressing mode configuration (locked by CFG_REGWEN)
- **DST_CONFIG** - Destination addressing mode configuration (locked by CFG_REGWEN)
- **HANDSHAKE_INTR_ENABLE** - Hardware handshake interrupt enable mask (locked by CFG_REGWEN)
- **CLEAR_INTR_SRC** - Interrupt clearing enable configuration (locked by CFG_REGWEN)
- **CLEAR_INTR_BUS** - Bus selection for interrupt clearing (locked by CFG_REGWEN)
- **INTR_SRC_ADDR_0** through **INTR_SRC_ADDR_10** - Interrupt clearing addresses (locked by CFG_REGWEN)
- **INTR_SRC_WR_VAL_0** through **INTR_SRC_WR_VAL_10** - Interrupt clearing write values (locked by CFG_REGWEN)

#### Hardware Handshake Register Arrays

For hardware handshake interrupt clearing configuration, register arrays use indexed callbacks:

| Callback Name | Type | Description |
|---|---|---|
| **handle_write_INTR_SRC_ADDR_n** | Write | Single callback with index parameter (n = 0-10). Stores 32-bit destination address for automatic interrupt clearing write when hardware handshake trigger N asserts. Used in conjunction with CLEAR_INTR_SRC[n] enable bit and CLEAR_INTR_BUS[n] bus selection. |
| **handle_write_INTR_SRC_WR_VAL_n** | Write | Single callback with index parameter (n = 0-10). Stores 32-bit write data value for automatic interrupt clearing transaction to peripheral INTR_SRC_ADDR_n when trigger N asserts. |

#### Callback Implementation Notes

##### Configuration Register Lock Enforcement
All registers protected by CFG_REGWEN must implement write callbacks that:
1. Check CFG_REGWEN current value (0x0 = locked, 0x6 = unlocked)
2. Return TLM_COMMAND_ERROR_RESPONSE if write attempted while locked
3. Update register value only when unlocked
4. This applies to: SRC_ADDR_LO/HI, DST_ADDR_LO/HI, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, HANDSHAKE_INTR_ENABLE, CLEAR_INTR_SRC, CLEAR_INTR_BUS, INTR_SRC_ADDR_n, INTR_SRC_WR_VAL_n

##### CONTROL Register Special Behavior
The CONTROL register write callback must handle multiple distinct operations:
1. **go bit (bit 31)**: Trigger DMA transfer state machine execution
2. **abort bit (bit 27)**: Initiate transfer abort (write-only, always reads 0)
3. **initial_transfer bit (bit 8)**: Reset SHA-2 hash state for new hash computation
4. **Hardware handshake mode**: go bit remains set until software clears it (exception to auto-clear rule)
5. **Normal mode**: go bit auto-clears after transfer completion

##### Address Register Dynamic Updates
Source and destination address registers may be dynamically updated during transfer execution:
- If SRC_CONFIG.increment=1: SRC_ADDR advances by TRANSFER_WIDTH after each read
- If DST_CONFIG.increment=1: DST_ADDR advances by TRANSFER_WIDTH after each write
- If wrap mode enabled: Address wraps to chunk start address after each chunk completion
- Read callbacks return current hardware address pointer, not initial programmed value

##### SHA-2 Digest Register Read Semantics
- SHA2_DIGEST_0 through SHA2_DIGEST_15 contain hash output after transfer completion
- STATUS.sha2_digest_valid indicates when digest data is valid
- Number of valid registers depends on CONTROL.opcode (SHA256/384/512 selection)
- CONTROL.digest_swap controls per-register endianness conversion
- Digest remains valid until next transfer with initial_transfer=1

#### TLM Modeling Implications

1. **State Machine Integration**: CONTROL.go write callback must trigger DMA state machine execution including configuration validation, address range checks, ASID validation, and data transfer initiation.

2. **Interrupt Management**: STATUS write callback must update internal interrupt state and call SystemC interrupt output port write methods to de-assert interrupt lines.

3. **Lock Enforcement**: All registers protected by CFG_REGWEN must check lock state in their write callbacks and return error response if locked.

4. **Address Advancement**: Address register read callbacks must reflect real-time transfer progress by tracking hardware address pointer state.

5. **Hash Engine Integration**: CONTROL.initial_transfer write must reset SHA-2 internal state. SHA2_DIGEST read must retrieve current hash engine output.

6. **Hardware Handshake**: lsio_trigger input port sensitivity must trigger chunk transfer execution and optional automatic interrupt clearing writes to peripheral INTR_SRC_ADDR registers.

7. **Error Reporting**: All configuration validation failures must populate ERROR_CODE register and set STATUS.error bit, triggering dma_error interrupt.

#### Security Callback Requirements

##### Memory Range Validation (Required in Multiple Callbacks)
- SRC_ADDR/DST_ADDR write callbacks must validate addresses against ENABLED_MEMORY_RANGE_BASE/LIMIT when crossing OpenTitan security boundary
- CONTROL.go write callback must perform comprehensive security checks:
  - Verify RANGE_VALID is set before allowing DMA operation
  - Enforce OT Private Memory / OT DMA Memory / SoC Memory access control matrix
  - Validate ASID combinations against security policy
  - Check address range containment for SoC-to-OT and OT-to-SoC transfers

##### ASID Validation
- ADDR_SPACE_ID write callback must validate multibit-encoded ASID values (0x7=OT_ADDR, 0x9=SYS_ADDR, 0xA=SOC_ADDR)
- Invalid ASID values must set ERROR_CODE.asid_error

##### Alignment Enforcement
- Address register write callbacks must validate alignment to TRANSFER_WIDTH:
  - ONE_BYTE (0x0): No alignment requirement
  - TWO_BYTE (0x1): Address[0] must be 0
  - FOUR_BYTE (0x2): Address[1:0] must be 0
- Misalignment sets ERROR_CODE.src_addr_error or ERROR_CODE.dst_addr_error

#### Notes
- All callback functions operate at TLM functional abstraction level
- Callbacks implement software-visible behavior, not RTL implementation details
- Register locking mechanisms (RANGE_REGWEN, CFG_REGWEN) are functionally modeled as access control checks
- Multibit encoding (0x6=True, 0x0=False) is abstracted to boolean logic in TLM
- Hardware handshake automatic interrupt clearing performs actual bus master write transactions through initiator sockets

---

## 4. Assumptions

The following assumptions define the boundaries and abstraction level of the DMA Controller SystemC TLM model:

### TLM Abstraction Level

1. **Loosely-Timed (LT) Modeling**: The model implements TLM-2.0 loosely-timed coding style with temporal decoupling. Timing points are annotated using `wait()` statements, but cycle-accurate timing is not guaranteed.

2. **Blocking Transport Interface**: All bus transactions use the TLM-2.0 blocking transport interface (`b_transport`). Non-blocking transport and Direct Memory Interface (DMI) are not supported.

3. **Timing Annotation**: Abstract timing delays are annotated for:
   - Register access latency (configurable via clock period)
   - Bus transaction latency (read/write cycle delays)
   - Data transfer duration (based on TOTAL_DATA_SIZE and bus bandwidth)
   - Hash computation latency (based on data size and hash algorithm)

4. **Temporal Decoupling**: The model supports temporal decoupling with quantum-based synchronization. Recommended quantum values:
   - Fast simulation: 1 ms - 10 ms
   - Moderate accuracy: 100 us - 1 ms
   - Software development: 10 us - 100 us

### Functional Abstraction

5. **Single Outstanding Transaction**: The DMA enforces strict sequential execution of bus transactions. No transaction pipelining or concurrent read/write operations are modeled. The next transaction begins only after the previous one completes.

6. **Atomic Bus Operations**: Each TLM transaction is atomic. Bus arbitration, split transactions, and protocol-level retry mechanisms are abstracted away.

7. **Sub-word Transfer Modeling**: Sub-word transfers (1-byte, 2-byte) are modeled using TLM byte-enable attributes. The model extracts the correct byte lane for reads and replicates data with appropriate byte-enables for writes, abstracting the underlying datapath multiplexing.

8. **Inline Hash Computation**: SHA-2 hash computation (SHA-256/384/512) is modeled as a functional operation without cycle-accurate computation timing. The hash result is produced atomically when the transfer completes, using standard cryptographic libraries (e.g., OpenSSL) rather than modeling hardware hash engine internals.

9. **Security Enforcement**: Address range validation, ASID-based access control, and RACL role checking are modeled as functional checks that generate errors. The underlying hardware security enforcement circuits (comparators, access control matrices) are not modeled at gate level.

10. **Multibit Encoding Abstraction**: Security registers using multibit encoding (RANGE_REGWEN, CFG_REGWEN) are abstracted to boolean logic. The specific bit patterns (0x6 = True, 0x0 = False) are validated functionally, but the redundant encoding for fault tolerance is not modeled.

### Hardware Abstraction

11. **Clock Modeling**: The `clk_i` input provides an abstract clock period for timing calculations. Clock edges, phase relationships, and clock domain crossing circuits are not modeled. A single synchronous clock domain is assumed.

12. **Clock Gating**: Functional clock gating is modeled as automatic power state transitions (active during transfers, idle otherwise). The physical clock gating cells (`prim_clock_gating`) and actual power consumption are not modeled.

13. **Reset Behavior**: The `rst_ni` input triggers functional reset of all registers and state machines. Asynchronous reset timing, reset synchronizers, and reset domain crossing circuits are abstracted. Reset is treated as an instantaneous state initialization.

14. **FSM Encoding**: The DMA state machine behavior (idle, configuration check, read, write, hash update, done, error) is modeled functionally. Sparse FSM encoding, one-hot encoding, and encoding for fault detection are not modeled.

15. **Register Implementation**: Registers are modeled as SystemC variables with callback functions for side effects. Physical register implementation details (flip-flop types, scan chains, multi-bit redundancy circuits) are not modeled.

### Protocol Abstraction

16. **TL-UL Protocol**: TileLink Uncached Lightweight (TL-UL) protocol is mapped to standard TLM-2.0 generic payload. Protocol-specific signals (A-channel, D-channel, opcode, param, size) are abstracted into TLM command, address, data, and byte-enable attributes.

17. **Custom System Bus**: The 64-bit custom system bus protocol is mapped to TLM generic payload with custom attributes in the extension mechanism. Protocol handshaking signals (vld_vec, grant_vec, opcode_vec) are abstracted into blocking transaction semantics.

18. **Bus Errors**: Bus errors from target devices are propagated via TLM response status (TLM_GENERIC_ERROR_RESPONSE or TLM_ADDRESS_ERROR_RESPONSE). The model detects these and populates the ERROR_CODE register, triggering the dma_error interrupt.

19. **Bus Arbitration**: Multi-master bus arbitration is outside the scope of this model. The interconnect/arbiter is assumed to handle arbitration and grant access to the DMA when transactions are issued.

### Interrupt Modeling

20. **Level-Sensitive Interrupts**: All interrupt outputs (dma_done_intr, dma_chunk_done_intr, dma_error_intr) are level-sensitive status interrupts. They remain asserted until explicitly cleared by software via STATUS register writes.

21. **Interrupt Latency**: Interrupt assertion latency is abstracted. Interrupts are asserted immediately when the triggering condition occurs (transfer done, error detected), without modeling physical signal propagation delays.

22. **Hardware Handshake Interrupts**: The `lsio_trigger` inputs are level-sensitive and trigger chunk transfers when enabled. The model does not simulate the internal FIFO state of peripheral devices; it relies on external stimulus to assert/de-assert these signals.

### Data Transfer Assumptions

23. **Sequential Chunk Processing**: In multi-chunk transfers, chunks are processed sequentially. The next chunk begins only after the previous chunk completes and (optionally) the chunk_done interrupt is acknowledged.

24. **Hardware Handshake Mode**: In hardware handshake mode, the DMA waits for the next `lsio_trigger` assertion before starting each chunk transfer. The model does not predict or simulate peripheral FIFO fill levels.

25. **Addressing Mode Behavior**:
    - **Increment mode**: Address advances by TRANSFER_WIDTH after each transaction
    - **Fixed mode**: Address remains constant (for FIFO-like peripherals)
    - **Wrap mode**: Address wraps to chunk base address at chunk boundaries

26. **Address Alignment**: Source and destination addresses must be aligned to TRANSFER_WIDTH. Misaligned addresses trigger an error before the transfer starts. No automatic address alignment or unaligned access support is provided.

27. **Transfer Width Constraints**: Transfer width (1, 2, or 4 bytes) is fixed for the entire transfer. Dynamic width adjustment within a transfer is not supported.

### Security Assumptions

28. **Memory Range Enforcement**: Address range checks (ENABLED_MEMORY_RANGE_BASE/LIMIT) are enforced only for OT internal address space (ASID = 0x7). Transfers entirely within SoC address spaces bypass range checks.

29. **ASID Validation**: Only three ASID values are valid: 0x7 (OT_ADDR), 0x9 (SYS_ADDR), 0xA (SOC_ADDR). Any other ASID value triggers an immediate error. No dynamic ASID remapping is supported.

30. **RACL Role Modeling**: RACL (Resource Access Control List) role identifiers are passed as transaction attributes on the system bus. Role validation is assumed to be performed by downstream targets (memory controllers, peripheral devices). The DMA does not interpret or enforce RACL policies beyond passing the role identifier.

31. **Range Lock Mechanism**: Once RANGE_REGWEN is written to 0x0 (locked), the memory range registers (ENABLED_MEMORY_RANGE_BASE, ENABLED_MEMORY_RANGE_LIMIT, RANGE_VALID) are permanently locked until the next system reset. No intermediate unlock mechanism exists.

32. **Configuration Lock Behavior**: CFG_REGWEN is hardware-managed and read-only for software. It automatically locks (0x0) when DMA becomes busy and unlocks (0x6) when DMA returns to idle. Software cannot manually lock/unlock CFG_REGWEN.

### Error Handling Assumptions

33. **Error Detection Priority**: Configuration errors (address misalignment, invalid opcode, zero size, ASID errors) are detected before the transfer begins. Bus errors are detected during transfer execution and immediately abort the transfer.

34. **Error Recovery**: When an error occurs, the DMA halts the transfer, sets STATUS.error and STATUS.aborted, populates ERROR_CODE, and asserts dma_error_intr. Software must clear the error and reconfigure the DMA before starting a new transfer.

35. **Abort Behavior**: Writing CONTROL.abort immediately halts the current transfer (if active). Partial data may be transferred. The STATUS.aborted bit is set, and dma_error_intr is not asserted for aborts (only for error conditions).

### Simulation Performance Assumptions

36. **Hash Computation Performance**: SHA-2 hash computation uses host-native cryptographic libraries. Simulation speed depends on host processor performance, not modeled hardware hash engine timing. For large transfers, hash computation may dominate simulation time.

37. **Memory Access Modeling**: The model does not simulate cache behavior, memory controller arbitration, or DRAM timing. Bus transactions complete in abstract time based only on annotated bus cycle delays.

38. **No Cycle-Accurate Timing**: The model is not suitable for performance analysis, timing verification, or power estimation. It is intended for software development, functional verification, and system-level integration testing.

### External Dependencies

39. **Peripheral Device Models**: Hardware handshake functionality requires peripheral device models (I2C, UART, SPI) to generate `lsio_trigger` signals. The DMA model does not simulate peripheral internal state.

40. **Interconnect Requirements**: The model assumes a TLM-compliant interconnect (bus matrix, crossbar) that routes transactions between the DMA's initiator sockets and target memory/peripheral models.

41. **Memory Models**: Source and destination addresses must resolve to valid TLM target sockets in the system. Memory regions must implement `b_transport` and return appropriate responses.

42. **System Initialization**: The model assumes system firmware or testbench initializes ENABLED_MEMORY_RANGE_BASE/LIMIT and RANGE_VALID before enabling DMA operations. Uninitialized security registers will cause transfers to fail with range_valid_error.

### Limitations

43. **No Multi-Channel Support**: The model is strictly single-channel. Multiple concurrent transfers are not supported. To model multi-channel behavior, instantiate multiple DMA models.

44. **No Burst Optimization**: Each data transaction is a single read or write. Burst transfers (multiple consecutive accesses) are not generated. This may underestimate performance compared to real hardware with burst support.

45. **No Pre-emption**: Once a transfer starts, it runs to completion (or abort/error). The model does not support transfer pre-emption or priority-based scheduling.

46. **No Power Management**: Beyond functional clock gating state, the model does not track power consumption, voltage domains, or power state transitions.

47. **No Fault Injection**: Hardware fault injection mechanisms (bit flips, stuck-at faults, protocol violations) are not modeled. The model assumes ideal hardware operation unless explicitly configured to generate errors via software.

---

## Notes

- The DMA controller is designed as a single-channel device, processing one transaction at a time
- The inline SHA-2 hashing feature is a unique characteristic that must be modeled functionally using cryptographic libraries
- Security enforcement through memory region isolation is critical for TLM accuracy and must validate all address ranges and ASID combinations
- Hardware handshaking mode enables autonomous operation with minimal CPU intervention, requiring proper modeling of lsio_trigger signal sensitivity
- The distinction between three bus interfaces (OT, CTN, System) is functionally significant and must be preserved in socket routing
- Error detection and reporting are comprehensive and must accurately populate ERROR_CODE register for all failure modes
- Clock gating is modeled as functional power management state transitions, not electrical implementation
- All TLM timing annotations are abstract and do not guarantee cycle-accurate behavior
