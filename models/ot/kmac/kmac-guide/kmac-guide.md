# KMAC SystemC/TLM – Brief Development Guide

## 1. Functional Abstraction
- Model KMAC/SHA-3 behavior functionally.
- Use **OpenSSL** for all cryptographic operations.
- Do not model internal Keccak rounds or timing.

## 2. Interface Modeling
- Replace TL-UL with a **TLM target socket** for MMIO access.
- Provide a **custom sc_interface** for Key Manager (bypassing MMIO).

## 3. State & Control
- Implement a **simple command-driven state machine**:
  `IDLE → ABSORB → FINALIZE → DONE`.
- State transitions occur on MMIO commands or service calls.

## 4. Data & Key Handling
- Buffer input message data internally.
- Support software-provided and KeyMgr-sideloaded keys.
- Zeroize keys on reset or error.

## 5. Timing Model
- No cycle accuracy.
- Use `b_transport` with optional **temporal decoupling** for nominal delays.

## 6. Feature Gaps
- Context save/restore, interrupts, and side-channel protections are **not modeled**.