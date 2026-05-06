# EDN (Entropy Distribution Network) Model High-Level Design Document

## TABLE OF CONTENTS

1. [Introduction](#1-introduction)
   - [1.1 Objective](#11-objective)
   - [1.2 Scope](#12-scope)
   - [1.3 Acronyms](#13-acronyms)
   - [1.4 Is list](#14-is-list)
   - [1.5 Is not list](#15-is-not-list)

2. [Functional Description](#2-functional-description)
   - [2.1 EDN Config. Parameters](#21-edn-config-parameters)
   - [2.2 Port interfaces](#22-port-interfaces)
   - [2.3 Memory-Mapped Registers](#23-memory-mapped-registers)

3. [Use model](#3-use-model)
   - [3.1 Callbacks on Memory-mapped registers/bit-fields](#31-callbacks-on-memory-mapped-registersbit-fields)

4. [Assumptions](#4-assumptions)

## 1. Introduction

### 1.1 Objective

This document provides the design specifications for the Entropy Distribution Network (EDN) as a SystemC TLM2 compliant model. This will be modeled at LT abstraction level with timing annotation and temporal decoupling. The intended use case of this model is software development. In this design we shall try to separate behavior, communication and timing as far as possible. The functional description of EDN along with the information about the internal registers and interface ports are discussed in detail.

### 1.2 Scope

The scope of the document is to describe the design details of the EDN. It mainly focuses on the implementation level details of the EDN that in turn would form the basis for developing the EDN LT model. Also, the interface details are discussed which would be used for communicating with the outside world.

### 1.3 Acronyms

| Acronym | Definition |
| --- | --- |
| **EDN** | Entropy Distribution Network |
| **CSRNG** | Cryptographically Secure Random Number Generator |
| **LT** | Loosely Timed |
| **TLM** | Transaction Level Modeling |
| **FIFO** | First-In First-Out |
| **FIPS** | Federal Information Processing Standards |
| **NIST** | National Institute of Standards and Technology |
| **DRBG** | Deterministic Random Bit Generator |
| **TL-UL** | TileLink Uncached Lightweight |
| **AES** | Advanced Encryption Standard |
| **CTR** | Counter Mode |

### 1.4 Is list

The EDN SystemC TLM model includes the following features:

#### Core Entropy Distribution Functionality
- Entropy distribution network serving as bridge between CSRNG and hardware peripherals
- Multiple peripheral endpoint interfaces supporting up to 8 concurrent connections
- Request/acknowledge handshake protocol for entropy requests from peripherals
- Data width conversion from 128-bit CSRNG blocks to 32-bit peripheral bus widths
- Internal buffering and data packing to satisfy peripheral requests before fetching more entropy
- Arbitration and prioritization logic for managing multiple simultaneous peripheral requests

#### Operating Modes
- Boot-time request mode for rapid entropy delivery at system startup
- Auto request mode with hardware-managed generate and reseed commands
- Software port mode for firmware-controlled command forwarding
- Mode transitions and state machine control via firmware configuration

#### CSRNG Interface and Command Management
- CSRNG application interface port commands: instantiate, generate, reseed, uninstantiate
- Command sequencing requirements per NIST SP 800-90A function envelopes
- Software command forwarding via SW_CMD_REQ register
- Command status tracking and acknowledgment from CSRNG
- Command FIFO management for generate and reseed commands in auto request mode
- MAX_NUM_REQS_BETWEEN_RESEEDS configuration for automatic reseed intervals

#### Interrupt Generation
- edn_cmd_req_done interrupt when software CSRNG request completes
- edn_fatal_err interrupt on FIFO error conditions
- Interrupt enable and status register management

#### Alert Generation
- Fatal alert for security errors including illegal state machine states, FIFO errors, counter errors, and TL-UL bus integrity failures
- Recoverable alert for entropy bus consistency check failures and field configuration errors
- Alert status registers ERR_CODE and RECOV_ALERT_STS

#### Error Detection and Handling
- FIFO error detection for read/write/state errors
- State machine illegal state detection for main and ack state machines
- Counter error detection with hardened counter validation
- Command FIFO errors for generate and reseed FIFOs
- CSRNG acknowledgment error detection
- Software-visible error status and classification

#### Multi-bit Encoding and Register Protection
- 4-bit multi-bit encoded fields in CTRL register for enhanced fault detection
- REGWEN write-protection mechanism for CTRL register
- Reserved register bit handling with read-as-zero semantics

#### FIPS Compliance Indicator
- FIPS status propagation from CSRNG to peripheral endpoints
- Per-endpoint FIPS indicator signal reflecting entropy compliance status
- Support for both FIPS-compliant and pre-FIPS entropy distributions

#### State Machine Management
- Main state machine with states: Idle, BootLoadIns, BootInsAckWait, BootLoadGen, BootGenAckWait, BootPulse, BootDone, BootLoadUni, BootUniAckWait, SWPortMode, AutoLoadIns, AutoFirstAckWait, AutoAckWait, AutoDispatch, AutoCaptGenCnt, AutoSendGenCmd, AutoCaptReseedCnt, AutoSendReseedCmd, RejectCsrngEntropy, CSRNGAckWait, Error
- State machine observability via MAIN_SM_STATE register
- Functional state transition modeling without cycle-accurate timing

#### Peripheral Endpoint Protocol
- 32-bit data bus per endpoint with data persistence until next request
- Request/acknowledge handshake supporting asynchronous peripherals
- FIPS indicator per endpoint for entropy compliance tracking

### 1.5 Is not list

The EDN SystemC TLM model explicitly excludes the following features:

#### Physical Entropy Generation
- Noise source generation mechanisms
- Analog entropy collection circuits
- Physical random number generator implementation
- Entropy conditioning hardware

#### Cryptographic Algorithm Implementation Details
- AES encryption algorithm internals within CSRNG
- CTR_DRBG state update function implementation
- NIST SP 800-90A algorithm step-by-step execution
- Key expansion and encryption round details

#### Side-Channel and Physical Security Circuits
- Power analysis resistance circuits
- Electromagnetic emission countermeasures
- Fault injection detection and protection
- Glitch detection hardware
- Temperature and voltage sensors for security monitoring

#### Detailed FIFO Implementation
- Exact pointer increment/decrement logic
- Memory array organization and addressing
- Gray code pointer synchronization for clock domain crossing
- Full/empty flag generation circuits

#### Manufacturing and Test Features
- Design-for-test scan chain insertion
- Built-in self-test circuits
- Manufacturing test modes
- Boundary scan implementation

#### Sparse State Machine Encoding Values
- Exact 9-bit sparse encoding values for main state machine
- Hamming distance properties between state encodings
- Redundant state representation for fault detection

#### Bus Protocol Physical Layer Timing
- Cycle-accurate TL-UL bus timing
- Setup and hold time constraints
- Signal slew rate characteristics
- Back-pressure exact cycle timing

#### FIPS Health Check Implementation
- Detailed health test algorithms within entropy_src
- Statistical test threshold values
- Continuous monitoring test specifics

As the model is not timing accurate it is not suitable for performance measurements. Pre-emption shall not be supported in the EDN Model due to the blocking transport nature of TLM LT model.

## 2. Functional Description

### 2.1 EDN Config. Parameters

After comprehensive analysis of the EDN hardware specification, **no build-time configuration parameters** have been identified for the SystemC TLM model. The EDN peripheral has a fixed hardware architecture with non-parameterizable attributes in the current implementation.

#### Fixed Hardware Attributes

The following are fixed constants in the EDN hardware design and are **NOT** build-time configuration parameters:

| Attribute | Value | Description |
|-----------|-------|-------------|
| Number of Endpoints | 8 | Fixed number of peripheral endpoint interfaces |
| Endpoint Bus Width | 32 bits | Fixed data bus width per peripheral endpoint |
| CSRNG Interface Width | 128 bits | Fixed genbits bus width from CSRNG |
| Command FIFO Depth | 13 words | Fixed depth for both GENERATE_CMD and RESEED_CMD FIFOs |

**Rationale for Exclusion:**

**Number of Endpoints (8)**: Documentation explicitly states that due to limitations in the parametrization of top-level interconnects this value is not currently parameterizable. However, the number of peripheral ports may change in a future revision. This is a fixed constant in the current implementation, not a parameter that varies across instantiations.

**Endpoint Bus Width (32 bits)**: Described as fixed bus width in multiple locations. Each hardware interface supports a fixed bus width of 32 bits. Not parameterizable; same for all endpoints.

**CSRNG Interface Width (128 bits)**: Determined by the CSRNG application interface specification. CSRNG will return 128 bits on the genbits bus. This is a property of the CSRNG interface, not an EDN configuration parameter.

**Command FIFO Depth (13 words)**: Fixed hardware depth to fill a FIFO with up to 13 command words. Not parameterizable across instantiations. Same for both GENERATE_CMD and RESEED_CMD FIFOs.

#### Runtime-Programmable Parameters

The following are configured at runtime via register writes and are **NOT** build-time parameters:

- **MAX_NUM_REQS_BETWEEN_RESEEDS**: Runtime-programmable register that sets the number of generate commands between reseed commands in auto request mode
- **BOOT_INS_CMD**: Runtime-programmable register for boot-time instantiate command
- **BOOT_GEN_CMD**: Runtime-programmable register for boot-time generate command
- **Operating Mode Configuration**: Selected at runtime via CTRL register fields (EDN_ENABLE, BOOT_REQ_MODE, AUTO_REQ_MODE)

#### SystemC TLM Model Implications

For the SystemC TLM implementation:

1. **Fixed Architecture**: The model should implement exactly 8 peripheral endpoint interfaces with 32-bit data buses.

2. **No Constructor Parameters Needed**: Since there are no build-time configuration parameters, the EDN SystemC constructor does not need parameterization for hardware structure.

3. **Runtime Configuration**: All operational configuration is performed through register writes to the TLM register interface, matching the hardware behavior.

4. **Future Extensibility**: If future EDN revisions introduce parameterizable endpoint counts, this document should be updated to include that parameter.

### 2.2 Port interfaces

This section defines the SystemC TLM port interfaces for the EDN (Entropy Distribution Network) IP. The EDN acts as an entropy distribution bridge between CSRNG (Cryptographically Secure Random Number Generator) and hardware peripherals that consume random data. The port definitions abstract hardware signals into transaction-level interfaces suitable for virtual platform integration.

#### IP Category

**Category**: Entropy Distribution Network / Security Peripheral

The EDN is a distribution network peripheral that bridges between CSRNG and hardware entropy consumers. It provides multiple peripheral endpoint interfaces, CSRNG application interface commands, and register-based configuration.

#### Port Interface Definitions

##### Register Bus Interface

| Interface Category | Port Name | Port Type | Description |
|---|---|---|---|
| Register Bus | tlm_reg_target_socket | tlm_target_socket<32> | TLM-2.0 register access interface for configuration and status registers (TL-UL protocol). Provides access to control registers (CTRL, REGWEN), command registers (SW_CMD_REQ, GENERATE_CMD, RESEED_CMD), status registers (SW_CMD_STS, HW_CMD_STS, MAIN_SM_STATE), interrupt registers (INTR_STATE, INTR_ENABLE, INTR_TEST), alert registers (ALERT_TEST, RECOV_ALERT_STS, ERR_CODE), and configuration registers (BOOT_INS_CMD, BOOT_GEN_CMD, MAX_NUM_REQS_BETWEEN_RESEEDS). |

##### CSRNG Application Interface

| Interface Category | Port Name | Port Type | Description |
|---|---|---|---|
| CSRNG Interface | csrng_cmd_initiator_socket | sc_port<csrng_app_if> | Initiator socket to CSRNG application interface for sending commands (instantiate, generate, reseed, uninstantiate). EDN acts as requester (req) for CSRNG application commands. Implements command sequencing per NIST SP 800-90A requirements. Handles command acknowledgments and status responses from CSRNG. |
| CSRNG Interface | csrng_genbits_target_socket | sc_export<csrng_genbits_if> | Target socket receiving 128-bit entropy data (genbits) from CSRNG. EDN receives generated random bits and buffers them for distribution to peripheral endpoints. |

##### Peripheral Endpoint Interfaces (8 Endpoints)

| Interface Category | Port Name | Port Type | Description |
|---|---|---|---|
| Endpoint Interface | edn_req[0:7] | sc_in<bool> | Endpoint request signals. Peripheral asserts to request fresh entropy. Array of 8 signals, one per endpoint. |
| Endpoint Interface | edn_ack[0:7] | sc_out<bool> | Endpoint acknowledge signals. EDN asserts when fresh entropy is available on corresponding edn_bus. Array of 8 signals, one per endpoint. |
| Endpoint Interface | edn_bus[0:7] | sc_out<sc_uint<32>> | Endpoint data buses. 32-bit entropy data output. Data persists until next request to support asynchronous endpoints. Array of 8 buses, one per endpoint. |
| Endpoint Interface | edn_fips[0:7] | sc_out<bool> | Endpoint FIPS indicators. Asserted when entropy meets NIST SP 800-90A FIPS compliance. De-asserted for pre-FIPS seeds. Array of 8 signals, one per endpoint. |

##### Interrupt Signals

| Interface Category | Port Name | Port Type | Description |
|---|---|---|---|
| Interrupts | intr_edn_cmd_req_done | sc_out<bool> | Software command request completion interrupt. Asserted when a software-initiated CSRNG request (via SW_CMD_REQ register) has completed. Event-type interrupt cleared by writing to INTR_STATE register. |
| Interrupts | intr_edn_fatal_err | sc_out<bool> | Fatal error interrupt. Asserted when a FIFO error occurs (FIFO read error, FIFO write error, generate command FIFO error, or reseed command FIFO error). Event-type interrupt cleared by writing to INTR_STATE register. |

##### Alert Signals

| Interface Category | Port Name | Port Type | Description |
|---|---|---|---|
| Security Alerts | alert_recov_alert | sc_out<bool> | Recoverable alert signal. Triggered when entropy bus data matches on consecutive clock cycles (EDN_BUS_CMP_ALERT), or when field configuration errors occur (CMD_FIFO_RST_FIELD_ALERT, AUTO_REQ_MODE_FIELD_ALERT, BOOT_REQ_MODE_FIELD_ALERT). Status visible in RECOV_ALERT_STS register. |
| Security Alerts | alert_fatal_alert | sc_out<bool> | Fatal alert signal. Triggered for illegal state machine states (EDN_MAIN_SM_ERR, EDN_ACK_SM_ERR), FIFO errors (SFIFO_GENCMD_ERR, SFIFO_RESCMD_ERR, FIFO_WRITE_ERR, FIFO_READ_ERR, FIFO_STATE_ERR), counter errors (EDN_CNTR_ERR), CSRNG acknowledgment errors (CSRNG_ACK_ERR), and TL-UL bus integrity failures. Status visible in ERR_CODE register. |

##### Clock and Reset

| Interface Category | Port Name | Port Type | Description |
|---|---|---|---|
| Clock | clk_i | sc_in<double> | Abstract clock input representing clock frequency in Hz. EDN uses single clock domain for all operations. Not cycle-accurate; temporal decoupling applied for simulation performance. |
| Reset | rst_ni | sc_in<bool> | Active-low asynchronous reset input. When asserted (low), EDN enters disabled state. Requires explicit enablement via EDN_ENABLE field in CTRL register after reset de-assertion. |

#### Interface Protocol Specifications

##### CSRNG Application Interface Protocol

**Direction**: EDN acts as initiator (master) to CSRNG

**Transaction Types**:
- **Instantiate Command**: Initialize CSRNG instance before generating entropy
- **Generate Command**: Request entropy generation (configurable length)
- **Reseed Command**: Refresh entropy seed periodically
- **Uninstantiate Command**: Destroy CSRNG instance for reconfiguration

**Command Format**: 32-bit command header followed by 0-12 additional 32-bit data words. Header specifies command type, flags, and additional data length (clen field).

**Acknowledgment Protocol**: CSRNG responds with acknowledgment signal and status code (success/error).

##### Peripheral Endpoint Interface Protocol

**Direction**: Peripheral initiates request, EDN responds with acknowledge and data

**Handshake Protocol**:
1. Peripheral asserts edn_req[i] signal (edge-sensitive)
2. EDN processes request (fetch from buffer or request from CSRNG if needed)
3. EDN asserts edn_ack[i] and drives edn_bus[i] with 32-bit entropy
4. EDN updates edn_fips[i] to reflect FIPS status
5. EDN maintains data on edn_bus[i] until next request (data persistence for asynchronous endpoints)

#### Implementation Notes

##### TLM Interface Types

**Custom Interface Definitions Required**:
- `csrng_app_if`: Abstract interface for CSRNG application commands (instantiate, generate, reseed, uninstantiate)
- `csrng_genbits_if`: Abstract interface for receiving 128-bit entropy from CSRNG

**Standard SystemC Types Used**:
- `tlm_target_socket<32>`: Standard TLM-2.0 target socket for register access
- `sc_in<bool>`, `sc_out<bool>`: Standard SystemC signal ports for interrupts, alerts, handshake signals
- `sc_out<sc_uint<32>>`: Standard SystemC signal port for 32-bit data buses
- `sc_in<double>`: Abstract clock frequency input (not cycle-accurate)

##### Signal Naming Conventions

Port names match hardware signal names from the datasheet for traceability:
- Register socket: `tlm_reg_target_socket` (maps to hardware `tl` interface)
- CSRNG interface: `csrng_cmd_initiator_socket` and `csrng_genbits_target_socket` (maps to hardware `csrng_cmd` interface)
- Endpoint signals: `edn_req[i]`, `edn_ack[i]`, `edn_bus[i]`, `edn_fips[i]` (maps to hardware `edn` port array)
- Interrupts: `intr_edn_cmd_req_done`, `intr_edn_fatal_err` (matches interrupt names)
- Alerts: `alert_recov_alert`, `alert_fatal_alert` (matches alert names)
- Clock/Reset: `clk_i`, `rst_ni` (matches OpenTitan comportability naming)

##### Exclusions (Not Modeled at TLM Level)

The following are explicitly excluded per TLM abstraction requirements:
- Pin-level timing (setup/hold times, signal slew rates)
- Cycle-accurate state machine transitions
- Exact FIFO depth implementation details
- Physical entropy generation mechanisms
- Side-channel and power analysis characteristics
- Sparse state machine encoding values
- Gate-level implementation details

### 2.3 Memory-Mapped Registers

The EDN (Entropy Distribution Network) module's registers are accessed through the peripheral bus at the module's base address. All configuration, status, and data transfer is handled via these registers.

#### Register Summary

| Register Name | Offset | Size (bits) | Access | Reset Value | Description/Notes |
|---|---|---|---|---|---|
| **Interrupt Registers** |
| INTR_STATE | 0x00 | 32 | RW1C | 0x00000000 | Interrupt state register with two interrupt sources: edn_cmd_req_done [0] and edn_fatal_err [1]. Reserved bits [31:2]. |
| INTR_ENABLE | 0x04 | 32 | RW | 0x00000000 | Interrupt enable register. Bits [1:0] enable corresponding interrupts in INTR_STATE. Reserved bits [31:2]. |
| INTR_TEST | 0x08 | 32 | WO | 0x00000000 | Interrupt test register. Writing 1 to bits [1:0] forces corresponding interrupt. Reserved bits [31:2]. |
| **Alert Registers** |
| ALERT_TEST | 0x0C | 32 | WO | 0x00000000 | Alert test register. Bits: recov_alert [0], fatal_alert [1]. Reserved bits [31:2]. |
| **Configuration and Control** |
| REGWEN | 0x10 | 32 | RW0C | 0x00000001 | Register write enable for CTRL register. Bit [0] when 1 allows CTRL writes, write 0 to lock. Reserved bits [31:1]. |
| CTRL | 0x14 | 32 | RW | 0x00009999 | EDN control register with multi-bit encoded fields. Bits [3:0]: EDN_ENABLE (reset 0x9), [7:4]: BOOT_REQ_MODE (reset 0x9), [11:8]: AUTO_REQ_MODE (reset 0x9), [15:12]: CMD_FIFO_RST (reset 0x9). Reserved bits [31:16]. Protected by REGWEN. Multi-bit fields accept 0x6 (enable) or 0x9 (disable). |
| **Boot-time Configuration** |
| BOOT_INS_CMD | 0x18 | 32 | RW | 0x00000901 | Boot instantiate command register. Full 32-bit CSRNG command word for boot-time instantiate. Hardware only supports clen=0. |
| BOOT_GEN_CMD | 0x1C | 32 | RW | 0x00FFF003 | Boot generate command register. Full 32-bit CSRNG command word for boot-time generate. Hardware only supports clen=0. |
| **Software Command Interface** |
| SW_CMD_REQ | 0x20 | 32 | WO | Undefined | Software command request register. Write CSRNG commands (up to 13 words) for firmware-controlled operation. |
| SW_CMD_STS | 0x24 | 32 | RO | 0x00000000 | Software command status register. Bits: CMD_REG_RDY [0], CMD_RDY [1], CMD_ACK [2], CMD_STS [5:3]. Reserved bits [31:6]. |
| **Hardware Command Status** |
| HW_CMD_STS | 0x28 | 32 | RO | 0x00000000 | Hardware command status register. Bits: BOOT_MODE [0], AUTO_MODE [1], CMD_TYPE [5:2], CMD_ACK [6], CMD_STS [9:7]. Reserved bits [31:10]. |
| **Auto Request Mode Configuration** |
| RESEED_CMD | 0x2C | 32 | WO | Undefined | Reseed command FIFO. Write CSRNG reseed commands (up to 13 words) for auto request mode. |
| GENERATE_CMD | 0x30 | 32 | WO | Undefined | Generate command FIFO. Write CSRNG generate commands (up to 13 words) for auto request mode. |
| MAX_NUM_REQS_BETWEEN_RESEEDS | 0x34 | 32 | RW | 0x00000000 | Counter limit for generate commands before automatic reseed in auto request mode. |
| **Status and Debug** |
| RECOV_ALERT_STS | 0x38 | 32 | RW0C | 0x00000000 | Recoverable alert status register. Write 0 to clear fields. Bits: EDN_ENABLE_FIELD_ALERT [0], BOOT_REQ_MODE_FIELD_ALERT [1], AUTO_REQ_MODE_FIELD_ALERT [2], CMD_FIFO_RST_FIELD_ALERT [3], EDN_BUS_CMP_ALERT [12]. Reserved bits remaining. |
| ERR_CODE | 0x3C | 32 | RO | 0x00000000 | Fatal error code register (sticky). Multiple error bits can be set. Requires module disable/enable to clear. Error types include state machine, FIFO, counter, and CSRNG acknowledgment errors. |
| ERR_CODE_TEST | 0x40 | 32 | WO | 0x00000000 | Error code test register. Writing 1 to any bit sets corresponding ERR_CODE bit for testing. |
| MAIN_SM_STATE | 0x44 | 32 | RO | 0x0000009E | Main state machine state register. 9-bit sparse encoded state value. States include Idle, BootLoadIns, SWPortMode, AutoLoadIns, etc. |

#### Register Field Details

##### Interrupt Registers (0x00 - 0x08)

**INTR_STATE (0x00)**
- **edn_cmd_req_done [0]**: RW1C - Software CSRNG request completion interrupt. Write 1 to clear.
- **edn_fatal_err [1]**: RW1C - FIFO fatal error interrupt. Write 1 to clear.

**INTR_ENABLE (0x04)**
- Enables corresponding interrupts in INTR_STATE register.

**INTR_TEST (0x08)**
- Writing 1 forces corresponding interrupt for testing purposes.

##### Alert Registers (0x0C)

**ALERT_TEST (0x0C)**
- **recov_alert [0]**: WO - Trigger recoverable alert (bus consistency, field encoding errors).
- **fatal_alert [1]**: WO - Trigger fatal alert (state machine errors, FIFO errors, counter errors, TL-UL integrity).

##### Configuration Registers (0x10 - 0x14)

**REGWEN (0x10)**
- **REGWEN [0]**: RW0C - Write protection for CTRL register. Default 1 (unlocked). Write 0 to lock permanently until reset.

**CTRL (0x14)** - Protected by REGWEN
- **EDN_ENABLE [3:0]**: Multi-bit encoded (0x6 = enable, 0x9 = disable). Enables EDN module. Must follow ENTROPY_SRC and CSRNG enable sequence.
- **BOOT_REQ_MODE [7:4]**: Multi-bit encoded (0x6 = enable, 0x9 = disable). Enables boot-time request mode (pre-FIPS entropy for fast boot).
- **AUTO_REQ_MODE [11:8]**: Multi-bit encoded (0x6 = enable, 0x9 = disable). Enables auto request mode with hardware-managed generate/reseed commands.
- **CMD_FIFO_RST [15:12]**: Multi-bit encoded (0x6 = reset, 0x9 = normal). Clears RESEED_CMD and GENERATE_CMD FIFOs. Must be cleared before issuing commands.

**Mode Priority**: BOOT_REQ_MODE > AUTO_REQ_MODE > Software Port Mode. Reserved bit behavior: Read as 0, writes ignored.

##### Boot-time Configuration (0x18 - 0x1C)

**BOOT_INS_CMD (0x18)**
- Full 32-bit CSRNG instantiate command for boot-time mode. Default 0x901 (instantiate, clen=0, no flags). Hardware limitation: clen must be 0.

**BOOT_GEN_CMD (0x1C)**
- Full 32-bit CSRNG generate command for boot-time mode. Default 0xFFF003 (generate, glen=0xFFF/4K blocks). Hardware limitation: clen must be 0.

##### Software Command Interface (0x20 - 0x24)

**SW_CMD_REQ (0x20)**
- Write-only FIFO for software-driven CSRNG commands. Supports instantiate, generate, reseed, uninstantiate. Each command consists of header word plus 0-12 additional data words. Poll SW_CMD_STS before each write.

**SW_CMD_STS (0x24)**
- **CMD_REG_RDY [0]**: Command register ready. 1 = ready to accept new command word via SW_CMD_REQ.
- **CMD_RDY [1]**: Command ready. 1 = ready to accept new multi-word command sequence.
- **CMD_ACK [2]**: Command acknowledged. 1 = CSRNG acknowledged current command.
- **CMD_STS [5:3]**: Command status code from CSRNG. 3-bit encoded status reflecting CSRNG response.

##### Hardware Command Status (0x28)

**HW_CMD_STS (0x28)**
- **BOOT_MODE [0]**: Boot-time request mode active.
- **AUTO_MODE [1]**: Auto request mode active.
- **CMD_TYPE [5:2]**: Current command type being processed by hardware (4-bit encoded).
- **CMD_ACK [6]**: Hardware command acknowledged by CSRNG.
- **CMD_STS [9:7]**: Hardware command status code from CSRNG. 3-bit encoded status.

##### Auto Request Mode Configuration (0x2C - 0x34)

**RESEED_CMD (0x2C)**
- Write-only FIFO for reseed command words used in auto request mode. Maximum 13 32-bit words. Overflow triggers fatal error.

**GENERATE_CMD (0x30)**
- Write-only FIFO for generate command words used in auto request mode. Maximum 13 32-bit words. Overflow triggers fatal error.

**MAX_NUM_REQS_BETWEEN_RESEEDS (0x34)**
- Counter limit for number of generate requests before automatic reseed command in auto request mode. When counter reaches this value, hardware automatically issues reseed command.

##### Status and Debug (0x38 - 0x44)

**RECOV_ALERT_STS (0x38)**
- **EDN_ENABLE_FIELD_ALERT [0]**: EDN_ENABLE field encoding error (not 0x6 or 0x9). Write 0 to clear.
- **BOOT_REQ_MODE_FIELD_ALERT [1]**: BOOT_REQ_MODE field encoding error. Write 0 to clear.
- **AUTO_REQ_MODE_FIELD_ALERT [2]**: AUTO_REQ_MODE field encoding error. Write 0 to clear.
- **CMD_FIFO_RST_FIELD_ALERT [3]**: CMD_FIFO_RST field encoding error. Write 0 to clear.
- **EDN_BUS_CMP_ALERT [12]**: Entropy bus consistency check failure (consecutive identical data). Write 0 to clear.

**ERR_CODE (0x3C)**
- Sticky read-only register recording fatal error codes. Multiple bits can be set simultaneously. Requires module disable/re-enable to clear. Error types include:
  - SFIFO_RESCMD_ERR: Reseed command FIFO error
  - SFIFO_GENCMD_ERR: Generate command FIFO error
  - FIFO_WRITE_ERR: FIFO write error
  - FIFO_READ_ERR: FIFO read error
  - FIFO_STATE_ERR: FIFO state error
  - EDN_ACK_SM_ERR: Acknowledge state machine illegal state
  - EDN_MAIN_SM_ERR: Main state machine illegal state
  - EDN_CNTR_ERR: Counter error (hardened counter validation failure)
  - CSRNG_ACK_ERR: CSRNG acknowledgment error

**ERR_CODE_TEST (0x40)**
- Write-only register for testing fatal error paths. Writing 1 to any bit position sets corresponding ERR_CODE bit.

**MAIN_SM_STATE (0x44)**
- Read-only register exposing current main state machine state. 9-bit sparse encoded value. States include:
  - Idle (0x9E)
  - BootLoadIns, BootInsAckWait, BootLoadGen, BootGenAckWait, BootPulse, BootDone, BootLoadUni, BootUniAckWait
  - SWPortMode
  - AutoLoadIns, AutoFirstAckWait, AutoAckWait, AutoDispatch, AutoCaptGenCnt, AutoSendGenCmd, AutoCaptReseedCnt, AutoSendReseedCmd
  - RejectCsrngEntropy, CSRNGAckWait
  - Error

#### Register Interactions and Dependencies

##### Initialization Sequence
1. Enable ENTROPY_SRC → CSRNG → EDN (in that order)
2. Write CTRL register to select operating mode
3. Optionally write REGWEN=0 to lock configuration

##### Boot-time Request Mode Sequence
1. Configure BOOT_INS_CMD and BOOT_GEN_CMD if non-default values needed
2. Write CTRL with EDN_ENABLE=0x6 and BOOT_REQ_MODE=0x6
3. Hardware automatically sends instantiate followed by generate commands
4. Exit by clearing BOOT_REQ_MODE, wait for MAIN_SM_STATE=SWPortMode state

##### Auto Request Mode Sequence
1. Write generate command words to GENERATE_CMD FIFO
2. Write reseed command words to RESEED_CMD FIFO
3. Write MAX_NUM_REQS_BETWEEN_RESEEDS counter value
4. Write CTRL with EDN_ENABLE=0x6 and AUTO_REQ_MODE=0x6
5. Issue SW instantiate command via SW_CMD_REQ
6. Hardware automatically manages generate/reseed thereafter
7. Exit by clearing AUTO_REQ_MODE, wait for command completion

##### Software Port Mode Sequence
1. Write CTRL with EDN_ENABLE=0x6, BOOT_REQ_MODE=0x9, AUTO_REQ_MODE=0x9
2. Poll SW_CMD_STS.CMD_RDY before starting new command
3. Poll SW_CMD_STS.CMD_REG_RDY before each command word
4. Write command header and data words to SW_CMD_REQ
5. Poll SW_CMD_STS.CMD_ACK to check completion
6. Check SW_CMD_STS.CMD_STS for success/error status

##### Multi-bit Encoded Field Values
- **Enable/True**: 0x6 (kMultiBitBool4True - binary 0110)
- **Disable/False**: 0x9 (kMultiBitBool4False - binary 1001)
- Any other value triggers recoverable alert via RECOV_ALERT_STS

##### Command FIFO Constraints
- Maximum 13 32-bit words per FIFO (RESEED_CMD, GENERATE_CMD)
- Command header specifies clen (additional data words count)
- clen must match actual words written (0-12 data words)
- Overflow (>13 words) triggers edn_fatal_err interrupt and fatal alert
- Reset FIFOs via CTRL.CMD_FIFO_RST before reuse

##### Error Handling
- **Recoverable errors** → RECOV_ALERT_STS register, recoverable alert signal
- **Fatal errors** → ERR_CODE register (sticky), edn_fatal_err interrupt, fatal alert signal
- Fatal errors require module disable/re-enable to clear

#### Legend

- **RO**: Read Only
- **RW**: Read/Write
- **WO**: Write Only
- **RW1C**: Read/Write 1 to Clear
- **RW0C**: Read/Write 0 to Clear
- **Reserved bits**: Read as 0, writes ignored, reserved for future use

## 3. Use model

EDN will be modeled at LT abstraction level with timing annotation and temporal decoupling. The intended use case of this model is software development.

### 3.1 Callbacks on Memory-mapped registers/bit-fields

This section specifies the register read/write callbacks required for the EDN (Entropy Distribution Network) SystemC TLM model. Callbacks are needed for registers that trigger hardware side-effects, state machine transitions, or implement special clearing modes beyond simple storage operations.

#### Callback Requirements Summary

The EDN IP requires callbacks for registers with:
- **Immediate functional side-effects**: State machine transitions, FIFO operations, command generation, interrupt/alert triggering
- **Conditional access dependencies**: Write protection (REGWEN), multi-bit encoding validation, mode-dependent behavior
- **Special clearing modes**: W1C (Write-1-to-Clear), W0C (Write-0-to-Clear)
- **Hardware action triggers**: CSRNG command generation, FIFO reset, mode transitions

#### Register Callbacks Table

| Callback Name | Type | Description |
|---|---|---|
| handle_write_INTR_STATE | Write | Clear interrupt bits using W1C mechanism. Writing 1 to edn_cmd_req_done [0] or edn_fatal_err [1] clears the corresponding interrupt state. Must check INTR_ENABLE to determine if interrupt output signal should be de-asserted. |
| handle_read_INTR_STATE | Read | Return current interrupt state reflecting hardware status: edn_cmd_req_done asserted on SW command completion, edn_fatal_err asserted on FIFO errors. |
| handle_write_INTR_ENABLE | Write | Update interrupt enable mask for edn_cmd_req_done [0] and edn_fatal_err [1]. Must update interrupt output signals if corresponding INTR_STATE bits are set. |
| handle_write_INTR_TEST | Write | Force interrupts for testing. Writing 1 to bits [1:0] sets corresponding INTR_STATE bits and asserts interrupt output if enabled in INTR_ENABLE. Does not store value (write-only register). |
| handle_write_ALERT_TEST | Write | Trigger alert outputs for testing. Writing 1 to recov_alert [0] triggers recoverable alert signal, writing 1 to fatal_alert [1] triggers fatal alert signal. Does not store value (write-only register). |
| handle_write_REGWEN | Write | Lock control register writes using W0C mechanism. Writing 0 to REGWEN [0] permanently locks CTRL register until reset. Once cleared to 0, subsequent writes to REGWEN have no effect (hardware enforced). Must gate all CTRL register write operations. |
| handle_write_CTRL | Write | Trigger state machine transitions and mode changes. Validate multi-bit encoded fields (EDN_ENABLE, BOOT_REQ_MODE, AUTO_REQ_MODE, CMD_FIFO_RST) are 0x6 or 0x9, otherwise set corresponding RECOV_ALERT_STS field and trigger recoverable alert. Check REGWEN before allowing write. EDN_ENABLE=0x6 enables module, =0x9 disables. BOOT_REQ_MODE=0x6 enters boot-time request mode (priority over AUTO_REQ_MODE), initiates instantiate then generate command sequence. AUTO_REQ_MODE=0x6 enters auto request mode after SW instantiate via SW_CMD_REQ. CMD_FIFO_RST=0x6 clears RESEED_CMD and GENERATE_CMD FIFOs, must return to 0x9 before issuing commands. Mode transitions wait for current command completion. De-asserting modes triggers state machine return to SWPortMode (boot mode auto-sends uninstantiate, auto mode requires SW uninstantiate). |
| handle_write_SW_CMD_REQ | Write | Accept CSRNG command words for software port mode. Check SW_CMD_STS.CMD_REG_RDY before accepting write. Parse command header (first word) to determine clen (additional data words). Accumulate clen+1 words then forward complete command to CSRNG via csrng_cmd_initiator_socket. Set SW_CMD_STS.CMD_ACK when CSRNG responds, update SW_CMD_STS.CMD_STS with CSRNG status code. Assert intr_edn_cmd_req_done interrupt on completion if INTR_ENABLE set. Does not store value (write-only FIFO input). |
| handle_read_SW_CMD_STS | Read | Return software command status. CMD_REG_RDY=1 when ready for next command word, CMD_RDY=1 when ready for new multi-word command, CMD_ACK reflects CSRNG acknowledgment, CMD_STS reflects CSRNG response status code. |
| handle_write_RESEED_CMD | Write | Accept reseed command words for auto request mode FIFO. Track FIFO word count (maximum 13 words). On overflow (>13 words), set ERR_CODE.SFIFO_RESCMD_ERR, assert intr_edn_fatal_err interrupt, trigger fatal_alert. Store command words for later retrieval by hardware state machine in auto request mode. Does not store value (write-only FIFO input). |
| handle_write_GENERATE_CMD | Write | Accept generate command words for auto request mode FIFO. Track FIFO word count (maximum 13 words). On overflow (>13 words), set ERR_CODE.SFIFO_GENCMD_ERR, assert intr_edn_fatal_err interrupt, trigger fatal_alert. Store command words for later retrieval by hardware state machine in auto request mode. Does not store value (write-only FIFO input). |
| handle_write_RECOV_ALERT_STS | Write | Clear recoverable alert status bits using W0C mechanism. Writing 0 to any field clears that alert condition. Must de-assert alert_recov_alert output if all status bits cleared. |
| handle_read_ERR_CODE | Read | Return current fatal error code register value (sticky). Multiple error bits can be set simultaneously. Value persists until module disable/re-enable. |
| handle_write_ERR_CODE_TEST | Write | Force fatal error conditions for testing. Writing 1 to any bit sets corresponding ERR_CODE bit, asserts intr_edn_fatal_err interrupt if enabled, triggers fatal_alert. Does not store value (write-only register). |
| handle_read_HW_CMD_STS | Read | Return hardware command status. BOOT_MODE and AUTO_MODE reflect current operating mode, CMD_TYPE shows command being processed, CMD_ACK shows CSRNG acknowledgment, CMD_STS reflects CSRNG response status. |
| handle_read_MAIN_SM_STATE | Read | Return current main state machine state (9-bit sparse encoded value). Reflects real-time state machine position for debug visibility. |

#### Exclusions

The following registers do NOT require callbacks (simple storage only):

##### Configuration Storage Registers
- **BOOT_INS_CMD**: Simple storage of 32-bit boot instantiate command value. Read by hardware state machine when entering boot-time request mode. No immediate side-effects on write.
- **BOOT_GEN_CMD**: Simple storage of 32-bit boot generate command value. Read by hardware state machine when entering boot-time request mode. No immediate side-effects on write.
- **MAX_NUM_REQS_BETWEEN_RESEEDS**: Simple storage of 32-bit counter value. Used by hardware in auto request mode to trigger automatic reseed after specified generate count. No immediate side-effects on write.

#### Callback Implementation Notes

##### Multi-bit Encoding Validation
CTRL register fields (EDN_ENABLE, BOOT_REQ_MODE, AUTO_REQ_MODE, CMD_FIFO_RST) use 4-bit multi-bit encoding:
- **0x6 (kMultiBitBool4True)**: Enable/True/Reset action
- **0x9 (kMultiBitBool4False)**: Disable/False/Normal operation
- **Any other value**: Illegal - set corresponding RECOV_ALERT_STS field and trigger recoverable alert

##### FIFO Management
- **RESEED_CMD and GENERATE_CMD FIFOs**: Maximum 13 32-bit words per FIFO
- **Overflow detection**: >13 words triggers SFIFO_RESCMD_ERR or SFIFO_GENCMD_ERR in ERR_CODE, edn_fatal_err interrupt, fatal alert
- **FIFO reset**: CTRL.CMD_FIFO_RST=0x6 clears both command FIFOs
- **Automatic clearing**: FIFOs cleared when EDN disabled, entering SWPortMode, boot sequence completion, or Idle state after auto mode

##### State Machine Coordination
- **Boot-time Request Mode**: BOOT_REQ_MODE=0x6 with EDN_ENABLE=0x6 triggers automatic instantiate command followed by continuous generate commands. Exit by clearing BOOT_REQ_MODE (auto-sends uninstantiate, transitions to SWPortMode).
- **Auto Request Mode**: AUTO_REQ_MODE=0x6 with EDN_ENABLE=0x6 after pre-loading GENERATE_CMD, RESEED_CMD FIFOs and setting MAX_NUM_REQS_BETWEEN_RESEEDS. Requires initial SW instantiate via SW_CMD_REQ. Hardware automatically issues generate commands on endpoint demand and reseed commands after counter expiry. Exit by clearing AUTO_REQ_MODE (transitions to SWPortMode, firmware must send uninstantiate).
- **Software Port Mode**: EDN_ENABLE=0x6 with BOOT_REQ_MODE=0x9 and AUTO_REQ_MODE=0x9. All commands issued via SW_CMD_REQ register. intr_edn_cmd_req_done interrupt asserted on command completion.
- **Mode Priority**: BOOT_REQ_MODE > AUTO_REQ_MODE > Software Port Mode

##### Interrupt and Alert Generation
- **Interrupts**: edn_cmd_req_done asserted on SW command completion (Software Port Mode only), edn_fatal_err asserted on FIFO errors
- **Alerts**: fatal_alert on state machine errors/FIFO errors/counter errors/TL-UL integrity failures, recov_alert on bus consistency errors/multi-bit encoding violations/CSRNG non-zero status
- **Clearing**: INTR_STATE uses W1C (write 1 to clear), RECOV_ALERT_STS uses W0C (write 0 to clear), ERR_CODE is read-only sticky (reset required)

##### Register Protection
- **REGWEN**: W0C write protection for CTRL register. REGWEN=1 allows CTRL writes, writing 0 locks CTRL permanently until reset. All handle_write_CTRL operations must check REGWEN=1 before accepting writes.

##### Command Sequencing Requirements
Per NIST SP 800-90A, CSRNG commands must follow proper ordering:
1. **Instantiate** required before any generate/reseed commands
2. **Generate** issued to obtain entropy (boot-time, auto mode, or SW port mode)
3. **Reseed** issued periodically (auto mode after MAX_NUM_REQS_BETWEEN_RESEEDS generates, or SW-controlled)
4. **Uninstantiate** required before reconfiguration or mode changes

#### Summary Statistics

- **Total Registers**: 17
- **Registers Requiring Callbacks**: 14
- **Excluded Registers (Storage Only)**: 3 (BOOT_INS_CMD, BOOT_GEN_CMD, MAX_NUM_REQS_BETWEEN_RESEEDS)
- **Write Callbacks**: 11 (INTR_STATE, INTR_ENABLE, INTR_TEST, ALERT_TEST, REGWEN, CTRL, SW_CMD_REQ, RESEED_CMD, GENERATE_CMD, RECOV_ALERT_STS, ERR_CODE_TEST)
- **Read Callbacks**: 5 (INTR_STATE, SW_CMD_STS, HW_CMD_STS, ERR_CODE, MAIN_SM_STATE)
- **Special Clearing Modes**: W1C (INTR_STATE), W0C (REGWEN, RECOV_ALERT_STS), Sticky-RO (ERR_CODE)

## 4. Assumptions

This section defines the modeling assumptions for the EDN SystemC TLM-2.0 implementation. These assumptions guide the abstraction level and implementation decisions for the virtual platform model, prioritizing functional accuracy over cycle-accurate timing to enable efficient firmware development and system integration verification.

### Modeling Assumptions

1. **TLM-2.0 loosely-timed abstraction** - Transaction-level interfaces using TLM-2.0 sockets; no cycle-accurate timing or pin-level signals modeled.

2. **Functional accuracy with temporal decoupling** - Maintains correct functional behavior and command sequencing order; uses quantum keeper for simulation performance; latency between requests and data availability is approximate, not cycle-by-cycle.

3. **Fixed 8-endpoint architecture** - Model implements exactly 8 peripheral endpoint interfaces with 32-bit data bus width per endpoint; no runtime parameterization of endpoint count.

4. **CSRNG dependency and interface abstraction** - EDN strictly distributes entropy; does not generate entropy (all random data originates from CSRNG model); CSRNG commands (instantiate, generate, reseed, uninstantiate) sent via TLM initiator socket; command acknowledgment and status responses modeled functionally.

5. **Data width conversion with behavioral buffering** - Converts 128-bit CSRNG genbits to 32-bit peripheral bus transactions; behavioral buffering ensures peripheral requests satisfied from internal buffer before fetching more data from CSRNG; buffering abstracted functionally, not with exact hardware FIFO implementation details.

6. **Three operating modes with state machine representation** - Boot-time request mode: automatic instantiate and generate commands for rapid startup entropy delivery; Auto request mode: hardware-managed generate and reseed with configurable MAX_NUM_REQS_BETWEEN_RESEEDS; Software port mode: firmware-controlled command forwarding via SW_CMD_REQ register; state transitions modeled functionally via main state machine (states: Idle, BootInsAckWait, SWPortMode, etc.) without sparse encoding values.

7. **Command FIFO functional modeling** - GENERATE_CMD and RESEED_CMD FIFOs model up to 13 command words (32-bit each); FIFO depth checking for overflow/underflow error conditions; FIFO behavior abstracted functionally without exact pointer logic or memory array implementation.

8. **Multi-bit encoding validation** - CTRL register fields (EDN_ENABLE, BOOT_REQ_MODE, AUTO_REQ_MODE, CMD_FIFO_RST) use 4-bit multi-bit encoding (0x6 enable, 0x9 disable); illegal values trigger recoverable alert; encoding provides basic fault detection without full sparse encoding implementation.

9. **Interrupt and alert generation** - Two interrupts: edn_cmd_req_done (SW command completion in Software Port Mode), edn_fatal_err (FIFO errors); Two alerts: fatal_alert (state machine errors, FIFO errors, counter errors, TL-UL integrity failures), recov_alert (entropy bus consistency errors, multi-bit encoding violations); interrupt/alert outputs modeled as SystemC sc_out<bool> signals.

10. **Register write protection with REGWEN** - REGWEN register provides W0C (Write-0-to-Clear) write protection for CTRL register; REGWEN=1 allows CTRL writes, writing 0 permanently locks CTRL until reset; protection enforced in register callback logic.

11. **FIPS status propagation** - FIPS compliance indicator from CSRNG propagated to each peripheral endpoint via edn_fips[0:7] signals; binary status (FIPS-compliant or pre-FIPS); no detailed FIPS health test implementation, only status pass-through.

12. **Error detection and status registers** - Fatal errors recorded in sticky ERR_CODE register (requires module disable/re-enable to clear); recoverable errors in RECOV_ALERT_STS (W0C clearing); error types include FIFO errors, state machine errors, counter errors, CSRNG acknowledgment errors.

13. **State machine state visibility** - MAIN_SM_STATE register exposes current main state machine state as 9-bit sparse encoded value; functional state names modeled (Idle, BootLoadIns, SWPortMode, AutoLoadIns, etc.); exact sparse encoding values are abstracted, state identity preserved for debug visibility.

14. **Command structure and parsing** - CSRNG commands consist of 32-bit header word followed by 0-12 additional data words (clen field in header specifies count); command types: instantiate (0x1), generate (0x3), reseed (0x4), uninstantiate (0x5); command parsing implemented in SW_CMD_REQ callback.

15. **Peripheral endpoint handshake abstraction** - edn_req[i] edge-sensitive request from peripheral, edn_ack[i] asserted by EDN when data ready, edn_bus[i] provides 32-bit entropy data; data persists on edn_bus[i] until next request (supports asynchronous peripherals); no cycle-accurate handshake timing modeled.

16. **Arbitration for multiple endpoints** - When multiple peripherals request simultaneously, EDN arbitrates based on functional priority or round-robin scheme; exact arbitration timing not modeled, order preserved functionally.

17. **Generate request counter in auto mode** - Tracks number of generate commands issued since last reseed; when count reaches MAX_NUM_REQS_BETWEEN_RESEEDS, hardware automatically issues reseed command; counter behavior modeled functionally without hardened counter redundancy details.

18. **Boot-time request mode automation** - When BOOT_REQ_MODE enabled, hardware automatically sends instantiate command using BOOT_INS_CMD value, followed by generate command using BOOT_GEN_CMD value; continuous generate commands until mode disabled; exit triggers automatic uninstantiate command.

19. **Software port mode command processing** - Firmware writes command words to SW_CMD_REQ register sequentially; model accumulates words until complete command received (based on clen field); forwards complete command to CSRNG; asserts intr_edn_cmd_req_done interrupt on completion.

20. **Auto request mode initialization** - Firmware pre-loads GENERATE_CMD and RESEED_CMD FIFOs with command words before enabling AUTO_REQ_MODE; firmware issues initial instantiate via SW_CMD_REQ; hardware then autonomously manages generate/reseed commands based on peripheral demand and reseed counter.

21. **Reset and initialization sequence** - EDN disabled after power-up; requires CTRL register write to enable; recommended enable sequence: ENTROPY_SRC, then CSRNG, then EDN; reset behavior modeled functionally without power-on timing or voltage ramp characteristics.

22. **TL-UL register interface abstracted to TLM** - TileLink-UL register bus abstracted to TLM-2.0 target socket (32-bit); bus integrity checking functionally validated (generates fatal alert on integrity failures); physical signaling, arbitration timing, and back-pressure exact cycle timing not modeled.

### Explicitly NOT Modeled

The following are explicitly excluded from the TLM abstraction:

- **Cycle-accurate timing** - Pin-level setup/hold times, signal slew rates, exact state machine transition cycles, bus protocol physical layer timing
- **Cryptographic algorithm internals** - AES encryption operations within CSRNG, DRBG state update details, CTR_DRBG algorithm implementation
- **Physical security features** - Side-channel resistance, power analysis signatures, electromagnetic emissions, fault injection protection circuits, temperature/process variations
- **Hardware implementation details** - Gate-level logic, sparse state encoding values (9-bit main FSM encoding), redundant counter implementations, FIFO pointer increment logic, memory array organization
- **Silicon-specific characteristics** - Technology node dependencies, aging/reliability effects, manufacturing test modes (DFT, BIST, scan chains)
- **Exact FIFO depth microarchitecture** - While FIFO depth (13 words) and error conditions are modeled, pointer logic and exact memory organization are abstracted
- **Reserved register bit handling** - Bit-level packing, read-modify-write hazards at bit level (functional register access is modeled)

### Modeling Scope Summary

This TLM model provides transaction-level functional accuracy suitable for:
- Firmware driver development and testing
- System integration verification (CSRNG -> EDN -> Peripherals entropy flow)
- Boot flow verification and mode transition validation
- Interrupt/alert handling validation
- Error condition testing and recovery

Not suitable for:
- Silicon verification or timing sign-off
- Side-channel analysis or power modeling
- Gate-level or RTL equivalence checking
