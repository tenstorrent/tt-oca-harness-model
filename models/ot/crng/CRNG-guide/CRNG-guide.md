# CSRNG (CRNG) SystemC/TLM Implementation Guide  
## Key Functional Changes from OpenTitan

## Overview
The CSRNG (Cryptographically Secure Random Number Generator) is modeled as the **central randomness engine** in the security subsystem.  
It is responsible only for **generating cryptographically secure random data** and does not distribute this data directly to hardware consumers. Distribution is handled by EDN.

---

## Key Functional Changes

### DRBG Implementation
- OpenTitan CSRNG implements **AES-256 CTR_DRBG (NIST SP 800-90A)** in RTL.
- In the SystemC/TLM model, this functionality is implemented using **OpenSSL `RAND_DRBG`**.
- Cryptographic behavior is functionally equivalent but **not bit-accurate**.

---

### Command FSM
- Supported commands:
  - `INSTANTIATE`
  - `RESEED`
  - `GENERATE`
  - `UPDATE`
  - `UNINSTANTIATE`
- The FSM is implemented using C++ control logic triggered by:
  - MMIO (software) accesses
  - Client requests (EDN)
- The model is **behavioral**, not cycle-accurate.

---

### Entropy Source
- In OpenTitan, CSRNG receives entropy from a dedicated Entropy Source IP.
- In the SystemC model:
  - Entropy input is **abstracted**
  - Entropy is supplied via OpenSSL (`RAND_poll()`) or a stub entropy provider
- Entropy health tests are **not modeled**.

---

### EDN Interface
- EDN acts as a **hardware client** of CSRNG.
- EDN issues CSRNG commands and receives generated random data.
- CSRNG does **not** directly interface with other hardware IPs.

---

### Software Interface
- OpenTitan CSRNG uses the TL-UL bus for register access.
- In the SystemC model, TL-UL transactions are mapped to **TLM-2.0 target socket MMIO transactions**.
- Register writes trigger CSRNG command execution.

---

## External Dependencies

- **Entropy Source**  
  Provides entropy during CSRNG instantiate and reseed operations.

- **EDN (Entropy Distribution Network)**  
  Acts as the sole hardware consumer of CSRNG-generated random data.

---

## Non-Responsibilities

The CSRNG SystemC/TLM model does not:
- Distribute random data directly to hardware IPs
- Perform entropy health testing
- Implement key derivation or cryptographic protocols
- Provide cycle-accurate timing or latency modeling
- Support context save/restore

---

## Timing Model
- The model is **not cycle-accurate**.
- Timing behavior is represented using **temporal decoupling (quantum keeper)**.
- Functional ordering of operations is preserved; exact latency is not.

---

## Intended Use
- Firmware and DIF development
- EDN and security IP integration
- System-level simulation and validation

---

## Not Intended For
- Cryptographic certification
- Entropy quality validation
- Side-channel or fault analysis

---

## Summary
The CSRNG SystemC/TLM model provides a functional abstraction of OpenTitan CSRNG using OpenSSL.  
It serves as the root randomness engine for the security subsystem, with EDN responsible for all hardware-facing entropy distribution.
