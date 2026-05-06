# Entropy Source IP – SystemC/TLM Functional Model (Abstract)

---

# 1. Purpose

This document defines a **highly abstract SystemC TLM functional model** of an Entropy Source IP.

The goal is to:
- Accurately model **register-visible behavior**
- Support **verification use-cases from the register specification**
- Avoid unnecessary micro-architectural detail

This model is a:
> Register-behavioral model (not a hardware-accurate entropy generator)

---

# 2. Modeling Philosophy

## 2.1 Core Rule

Only model behavior that is **observable via registers or interrupts**.

Do NOT model:
- Physical entropy generation
- Ring oscillator behavior
- Statistical correctness of randomness
- Exact hardware pipeline stages

---

## 2.2 Allowed Abstractions

- Entropy = pseudo-random values
- Health tests = counter increments (no real math)
- Timing = loosely approximated
- Internal structure = simplified

---

# 3. External Interface

## 3.1 TLM Register Interface

- Protocol: TLM-2.0
- Interface: Target socket
- Method: `b_transport`

### Note:
- **Register storage, field decoding, and access policies are handled by CSML**
- This model interacts with registers via:
  - Read callbacks / hooks
  - Write callbacks / hooks

### Responsibilities of this model:
- Implement **side effects of register access**
- React to register changes
- Update internal state accordingly

---

## 3.2 Interrupt Interface

- Logical interrupt output
- Derived from:
  - INTR_STATUS
  - INTR_ENABLE

Interrupt bits:
- [0] HEALTH_TEST_FAILED
- [4] FIFO_ERROR
- [8] FIFO_OVERFLOW
- [12] FIFO_UNDERFLOW

---

# 4. Internal Model Structure

The model consists of:

- FIFO queue
- Counters (health status)
- Interrupt flags
- Background process

Registers themselves are **not stored in this model** (handled by CSML).

---

# 5. Register Interaction Model

## 5.1 General Approach

- CSML manages:
  - register storage
  - default values
  - write masks
  - access types (RO/RW/WO/W1C)

- This model:
  - observes register changes
  - implements behavioral side effects

---

## 5.2 Write Side Effects

Triggered via write callbacks:

- CTRL[0] → triggers reset
- INTR_TEST → inject interrupt
- HEALTH_TEST_CTRL → enable/disable counters
- FIFO_CTRL → enable/disable FIFO

---

## 5.3 Read Side Effects

Triggered via read callbacks:

- FIFO_RDATA:
  - Pop one entry from FIFO
  - Update FIFO status
  - If empty → trigger underflow

---

# 6. FIFO Model

## 6.1 Description

- FIFO is an abstract queue storing entropy values

## 6.2 Depth

- consider 127


## 6.3 Behavior

- Background process pushes data into FIFO
- Read from FIFO_RDATA:
  - Pops one entry
  - Reduces FIFO level

## 6.4 Empty Condition

- If FIFO is empty:
  - Return 0
  - Set FIFO_UNDERFLOW interrupt

---

# 7. Entropy Generation (Abstract)

- No physical modeling required
- Use pseudo-random generator

### Behavior:
- Periodically push values into FIFO
- Controlled by enable conditions

---

# 8. Health Test Behavior (Abstract)

## 8.1 Counters

- Increment when health tests are enabled
- Stop when disabled
- Reset clears all counters

## 8.2 No Statistical Modeling

- No real APT / Markov computation required

## 8.3 Failure Handling

- Optional:
  - Can trigger HEALTH_TEST_FAILED interrupt

---

# 9. Interrupt Logic

## 9.1 INTR_STATUS

- Set by internal events
- Cleared via W1C behavior (handled by CSML)

## 9.2 INTR_ENABLE

- Masks interrupt output

## 9.3 INTR_TEST

- Write-only (handled by CSML)
- Write triggers interrupt injection

---

# 10. Reset Behavior

## 10.1 Software Reset

Triggered by:
- CTRL[0] = 1

## 10.2 Effects

- FIFO cleared
- Counters reset
- Internal state cleared

Register reset values are handled by CSML.

---

# 11. Background Process

A continuous process shall simulate internal activity:

loop:
    if enabled:
        generate pseudo-random value
        push into FIFO (if space available)
        update counters (if health tests enabled)

---

# 12. TLM Behavior

## 12.1 Role of TLM Layer

- Provide transport interface
- Forward transactions to CSML register layer
- Trigger callbacks for side effects

## 12.2 No Direct Register Storage

- All register reads/writes are delegated to CSML
- This model reacts via hooks

---

# 13. Error Conditions

Model must support:

- FIFO_UNDERFLOW
- FIFO_OVERFLOW (optional)
- HEALTH_TEST_FAILED (optional trigger)

---

# 14. Timing

- No cycle accuracy required
- Use simple periodic delays

---

# 15. Verification Alignment

Model must support:

- Default value checks (via CSML)
- RW pattern tests (via CSML)
- Write mask validation (via CSML)
- W1C behavior (via CSML)
- FIFO read side effects
- Counter updates

As required by the specification :contentReference[oaicite:0]{index=0}  

---

# 16. Summary

This model is:

- A **behavioral layer on top of CSML register model**
- A **state-driven abstraction**
- A **TLM-compatible functional model**

NOT a hardware-accurate entropy generator.
