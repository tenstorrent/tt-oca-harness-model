# EDN (Entropy Distribution Network) SystemC/TLM Implementation Guide

## Overview
The EDN (Entropy Distribution Network) is modeled as the **entropy distribution prioritization and interface bridge** in the security subsystem.
It serves as the intermediary between the CSRNG (Cryptographically Secure Random Number Generator) and hardware peripherals that consume random data, such as the Key Manager, OTBN, and others.

---

## Key Functional Changes

### Interface Abstraction
- In OpenTitan hardware, EDN distributes entropy via dedicated `req`/`ack` interface buses and a shared `genbits` data bus.
- In the SystemC/TLM model, these connections are implemented using **TLM-2.0 sockets**.
  - **Peripheral Interface**: Peripherals connect to EDN target sockets to request entropy.
  - **CSRNG Interface**: EDN uses an initiator socket to send high-level commands (Instantiate, Generate, Reseed) to the CSRNG.

### Data Handling
- Hardware manages width conversion (converting 128-bit CSRNG blocks to 32-bit peripheral bus widths).
- The SystemC model behaviorally manages this buffering and packing, ensuring peripheral requests are satisfied from the internal buffer before fetching more data from CSRNG.

### Operating Modes
- **Boot-time Request Mode**:
  - Hardware: Automatically sends a fixed sequence of commands to CSRNG at reset to populate entropy for boot logic.
  - SystemC Model: Implements the logic to trigger these initial CSRNG transactions automatically upon reset/configuration, ensuring boot ROMs and early-boot peripherals receive data.
- **Auto Request Mode**:
  - Hardware: Uses hardware FIFOs and timers to automatically reseed and generate data without software intervention.
  - SystemC Model: Uses C++ control logic to monitor buffer levels and issue `GENERATE` or `RESEED` commands to the CSRNG model when thresholds are met.
- **Software Mode**:
  - Supports firmware-driven command forwarding via registers (`SW_CMD_REQ`), mapped to TLM registry accesses.

---

## External Dependencies

- **CSRNG**
  The upstream source of all entropy. EDN cannot function without a connected and responsive CSRNG model.

- **Entropy Consumers (Peripherals)**
  Hardware blocks (e.g., KMAC, OTBN, AES, Alert Handler) that require random numbers connect to EDN ports.

---

## Technical constraints & Non-Responsibilities

The EDN SystemC/TLM model does not:
- **Generate Entropy**: It is strictly a distributor. All random data comes from CSRNG.
- **Perform Cryptographic Checks**: FIPS consistency checks (CRNGT) on the bus are abstracted or assumed usage-correct.
- **Guarantee Cycle Accuracy**: Latency between a request and data availability is modeled approxiamtely, not cycle-by-cycle.
- **Model Analog/Physics**: No physical entropy source noise modeling (delegated to CSRNG/Entropy Source).

---

## Timing Model
- The model is **not cycle-accurate**.
- Uses **temporal decoupling** (quantum keeper) for simulation performance.
- Maintains functional ordering of requests and arbitration between multiple peripheral ports.

---

## Intended Use
- **Firmware Development**: Developing and testing EDN drivers, configuring queues, and managing interrupt/alert handling.
- **System Integration**: Verifying the connection and flow of entropy from CSRNG -> EDN -> Peripherals.
- **Boot Flow Verification**: Validating that boot-time entropy is delivered correctly to start the system.

---

## Not Intended For
- **Silicon Verification**: Should not be used to sign off on timing paths or signal integrity.
- **Side-Channel Analysis**: Power and EM signatures are not modeled.

---

## Summary
The EDN SystemC/TLM model provides a functional abstraction of the OpenTitan EDN. It manages the complex interfacing with CSRNG and provides a simplified, prioritized distribution network for random data to the rest of the system, enabling efficient firmware development and system-level validation.
