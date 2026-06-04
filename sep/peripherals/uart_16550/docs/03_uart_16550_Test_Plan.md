# UART 16550 SystemC TLM2.0 Test Plan

**Document Version:** 1.0  
**Date:** 2025-10-29  
**IP Name:** UART 16550  
**Target:** SystemC TLM2.0 Model Verification  

---

## 1. Introduction

### 1.1 Purpose

This test plan defines the verification strategy for the UART 16550 SystemC TLM2.0 model, covering functional verification, register access validation, interrupt handling, FIFO operation, Loopback feature, and CCI-based configuration.

### 1.2 Verification Scope

The verification focuses on:
- Register groups, aliasing, and register callbacks
- FIFO operation with extended trigger levels (1, 4, 8, 14, 32, 64, 128, 256, 512, 1024, 2048, 4096)
- FIFO edge cases: empty FIFO read behavior
- Interrupt generation and handling: RDA, THRE, Line Status, plus test-forced interrupts (ITR)
- Reset behavior and initialization
- Data transmission and reception
- Line status monitoring for polling
- Loopback features: internal LOOP and LINE_LOOPBACK with precedence behavior
- Error conditions: Overrun Error (OE) in non-FIFO and FIFO modes
- **CCI Configuration**: Verification of parameter loading via JSON
- **Temporal Decoupling**: Verification of quantum keeper functionality

### 1.3 Verification Methodology

The verification uses a SystemC TLM2.0 testbench with:
- Directed test cases for specific feature validation
- Register access verification
- Register aliasing verification
- Interrupt service routine testing
- FIFO threshold testing
- CCI parameter inspection and validation

### 1.4 Success Criteria

Verification is complete when:
- All test cases pass without errors
- All registers are accessed correctly
- All interrupts assert and deassert as specified
- All error conditions are detected and reported
- FIFO operation matches specification
- Data integrity is maintained during transfers
- Loopback feature is verified
- CCI parameters are correctly loaded and applied

---

## 2. Test Case Organization

Test cases are organized into the following categories:
1. Reset and Initialization Tests
2. Register Access Tests
3. Data Transfer Tests
4. FIFO Operation Tests
5. Interrupt Tests
6. Loopback feature Tests
7. CCI Configuration Tests (Implicitly covered by running tests with/without config)

---

## 3. Test Cases

| Case | Test Case Name | Description | Registers Programmed | Ports/Signals Used | Test Type |
|------|----------------|-------------|----------------------|-------------------|-----------|
| 0 | test_reset_registers | Verify all registers return to default values after reset | All key regs (RBR, THR, IER, IIR, LCR, LSR, FCR) | reset | Positive |
| 1 | test_dll_access_dlab1 | DLL access when DLAB=1 | LCR(DLAB=1), DLL | TLM (b_transport) | Positive |
| 2 | test_dlm_access_dlab1 | DLM access when DLAB=1 | LCR(DLAB=1), DLM | TLM (b_transport) | Positive |
| 3 | test_thr_access_dlab0 | THR write when DLAB=0 | LCR(DLAB=0), THR | TLM (b_transport) | Positive |
| 4 | test_dll_readback | DLL readback after THR write (aliasing check) | LCR(DLAB=1), DLL | TLM (b_transport) | Positive |
| 5 | test_dlm_readback | DLM readback after THR write (aliasing check) | LCR(DLAB=1), DLM | TLM (b_transport) | Positive |
| 6 | test_rx_polling | RX polling via LSR.DR then read RBR | LCR(DLAB=0), LSR, RBR | terminal_to_uart() | Positive |
| 7 | test_tx_fifo_8bytes | XMIT FIFO TX with 8 bytes | FCR(FIFO=1, resets, trig=8) | TX path | Positive |
| 8 | test_tx_fifo_1byte | XMIT FIFO TX with 1 byte | FCR(FIFO=1) | TX path | Positive |
| 9 | RDA FIFO intr trig=1 (1 byte) | RDA interrupt on RCVR FIFO trig=1 | IER(ERBFI), ECR(0x0), FCR(trig LSB=0) | INTR | Positive |
| 10 | RDA FIFO intr trig=4 (4 bytes) | RDA interrupt on RCVR FIFO trig=4 | IER(ERBFI), ECR(0x0), FCR(trig LSB=1) | INTR | Positive |
| 11 | RDA FIFO intr trig=8 (8 bytes) | RDA interrupt on RCVR FIFO trig=8 | IER(ERBFI), ECR(0x0), FCR(trig LSB=2) | INTR | Positive |
| 12 | RDA FIFO intr trig=14 (14 bytes) | RDA interrupt on RCVR FIFO trig=14 | IER(ERBFI), ECR(0x0), FCR(trig LSB=3) | INTR | Positive |
| 13 | RDA FIFO intr trig=32 | Extended trigger via ECR MSBs | IER(ERBFI), ECR(0x1), FCR(trig LSB=0) | INTR | Positive |
| 14 | RDA FIFO intr trig=64 | Extended trigger | IER(ERBFI), ECR(0x1), FCR(trig LSB=1) | INTR | Positive |
| 15 | RDA FIFO intr trig=128 | Extended trigger | IER(ERBFI), ECR(0x1), FCR(trig LSB=2) | INTR | Positive |
| 16 | RDA FIFO intr trig=256 | Extended trigger | IER(ERBFI), ECR(0x1), FCR(trig LSB=3) | INTR | Positive |
| 17 | RDA FIFO intr trig=512 | Extended trigger | IER(ERBFI), ECR(0x2), FCR(trig LSB=0) | INTR | Positive |
| 18 | RDA FIFO intr trig=1024 | Extended trigger | IER(ERBFI), ECR(0x2), FCR(trig LSB=1) | INTR | Positive |
| 19 | RDA FIFO intr trig=2048 | Extended trigger | IER(ERBFI), ECR(0x2), FCR(trig LSB=2) | INTR | Positive |
| 20 | RDA FIFO intr trig=4096 | Extended trigger | IER(ERBFI), ECR(0x2), FCR(trig LSB=3) | INTR | Positive |
| 21 | RDA RBR intr (non-FIFO) | RDA interrupt in non-FIFO mode | IER(ERBFI), FCR(FIFO=0) | INTR | Positive |
| 22 | THRE intr (non-FIFO) | THR Empty interrupt | IER(ETBEI), FCR(FIFO=0), THR | INTR | Positive |
| 23 | THRE intr (FIFO) | THR Empty interrupt with FIFO enabled | IER(ETBEI), FCR(FIFO=1), THR | INTR | Positive |
| 24 | System LOOP (polling) | Internal loopback, poll LSR.DR | IER=0, MCR(LOOP) | RX/TX | Positive |
| 25 | System LOOP intr (non-FIFO) | Internal loopback with RDA intr | IER(ERBFI), FCR(FIFO=0), MCR(LOOP) | INTR | Positive |
| 26 | System LOOP intr (FIFO) | Internal loopback with FIFO RDA intr | IER(ERBFI), FCR(FIFO=1), MCR(LOOP) | INTR | Positive |
| 27 | ITR TRBFI | Force RX Data Ready via ITR | ITR(TRBFI) | INTR | Positive |
| 28 | ITR TTBEI | Force THR Empty via ITR | ITR(TTBEI) | INTR | Positive |
| 29 | ITR TLSI | Force Line Status via ITR | ITR(TLSI) | INTR | Positive |
| 30 | ITR TDSSI | Force Modem Status via ITR | ITR(TDSSI) | INTR | Positive |
| 31 | ITR TFEI | Force FIFO Error via ITR | ITR(TFEI) | INTR | Positive |
| 32 | ITR TRTI | Force Character Timeout via ITR | ITR(TRTI) | INTR | Positive |
| 33 | LINE_LOOPBACK intr | Reflect external line internally; RDA intr expected | IER(ERBFI), FCR, MCR(LINE_LOOPBACK) | INTR | Positive |
| 34 | LOOP + LINE_LOOPBACK | Both bits set; precedence behavior (no double RX) | MCR(LOOP|LINE_LOOPBACK) | INTR/RX | Positive |
| 35 | OE non-FIFO | Overrun error in non-FIFO mode | IER(ERBFI off, ELSI on), FCR(FIFO=0) | LSR/INTR | Negative |
| 36 | OE FIFO (overflow) | Overrun error by writing 4097 bytes into 4096-depth FIFO | IER(ERBFI off, ELSI on), ECR/FCR(FIFO=1) | LSR/INTR | Negative |
| 37 | Empty RCVR FIFO Read | Read from empty RCVR FIFO in FIFO mode; verify value=0, LSR.DR=0, no RDA interrupt | IER(ERBFI), FCR(FIFO=1, resets) | LSR, RBR, INTR | Positive |

---

## 4. Test Environment

### 4.1 Dependencies
- SystemC 2.3.3 or later
- C++17 compatible compiler
- CMake 3.14 or later
- **CCI 1.0.0** (or compatible)

### 4.2 Testbench Components
- UART 16550 DUT (Device Under Test)
- Test controller
- gnome-terminal terminal emulator (to simulate Serial output and input to UART)
- SystemC TLM2.0 interfaces
- **CCI Broker** for parameter management

### 4.3 Building and Running Tests

```bash
# Configure and build
mkdir -p build && cd build
cmake ..
make

# Run all tests (Default: Quantum Keeper Disabled)
./release/uart_test or ./debug/uart_test

# Run with CCI Configuration (Quantum Keeper Enabled)
./release/uart_test ../config/uart_default.json or ./debug/uart_test ../config/uart_default.json
