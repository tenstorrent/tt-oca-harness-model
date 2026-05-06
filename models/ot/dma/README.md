# DMA SystemC Model

## Overview

This directory contains a SystemC transaction-level model (TLM) of the OpenTitan DMA controller.
The model focuses on software-visible behavior for register programming, transfer execution, interrupt generation, and error handling.

Key capabilities include:

- Memory-to-memory, memory-to-peripheral, and peripheral-to-memory transfers
- Multiple addressing modes (increment, fixed, wrap)
- Configurable transfer width (1, 2, or 4 bytes)
- Multi-bus transfer routing through ASID-based source/destination selection
- Inline SHA-2 hashing (SHA-256, SHA-384, SHA-512) using OpenSSL
- Hardware handshake support with 11 trigger inputs
- Interrupt outputs for done, chunk_done, and error events
- Security checks for address range and transfer policy enforcement

## Directory Structure

```text
models/ot/dma/
├── model/
│   ├── src/                # DMA model implementation
│   └── inc/                # DMA headers
├── test/
│   ├── src/                # Testbench and functionality tests
│   └── inc/                # Test headers
├── dma-knowledge-base/     # Design and behavior documentation
├── docs/                   # Generated and curated DMA docs
├── CMakeLists.txt          # Build configuration
└── README.md
```

## Prerequisites

- SystemC (3.0.1 or later recommended)
- CMake (3.14+)
- C++17 compiler
- OpenSSL development package (required for SHA-2 support)
- Optional: lcov/gcov for coverage

Environment variables commonly used:

```bash
export SYSTEMC_HOME=/usr/local/systemc301
# Optional if CCI is installed in a custom location
export CCI_HOME=/path/to/cci
```

## Building the Model

The DMA model uses **CMake** as its build system.

### CMake Build Types

**Debug Build** - Full logging, debug symbols:

```bash
cd models/ot/dma
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
./debug/dma_testbench
```

**Release Build** - Optimized, errors-only logging:

```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
make
./release/dma_testbench
```

**ASAN Build** - Memory error detection with sanitizers:

```bash
cmake -DCMAKE_BUILD_TYPE=ASAN ..
make
./asan/dma_testbench
```

**Coverage Build** - Coverage-enabled instrumentation:

```bash
cmake -DCMAKE_BUILD_TYPE=Coverage ..
make
make coverage  # Runs tests and generates HTML coverage report
firefox coverage/html/index.html
```

When tests are enabled (`BUILD_TESTS=ON`, default for standalone build), the main executable is:

```text
build/<type>/dma_testbench
```

## Run Tests

```bash
cd models/ot/dma/build
./debug/dma_testbench
```

The testbench runs DMA functional scenarios (`FUNC-001` through `FUNC-011`) and prints a pass/fail summary at the end of simulation.

## Model Interfaces

- Register interface: TLM target socket
- DMA data interfaces:
  - `ot_initiator_socket` (OT internal bus)
  - `ctn_initiator_socket` (control network bus)
  - `sys_initiator_socket` (system bus)
- Interrupt outputs:
  - `dma_done_intr`
  - `dma_chunk_done_intr`
  - `dma_error_intr`
- Alert output:
  - `alert_fatal_fault`
- Handshake inputs:
  - `lsio_trigger[11]`

## Documentation

Detailed documentation is available in:

- `dma-knowledge-base/dma.md`
- `dma-knowledge-base/theory_of_operation.md`
- `dma-knowledge-base/programmers_guide.md`
- `dma-knowledge-base/registers.md`
- `docs/dma-high-level-design.md`
- `docs/dma-detailed-design.md`
- `docs/dma-test-plan.md`

## Notes

- The model links against OpenSSL for inline hash computation.
- CSML support is integrated via `../../utils/csml`.
- For VP/integration builds, tests can be disabled with `-DBUILD_TESTS=OFF`.
