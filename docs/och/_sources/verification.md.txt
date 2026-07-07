# Verification and Validation

## Verification Scope

The local OCH tree contains verification collateral for the major subsystems:

| Area | Directory | Coverage intent |
| --- | --- | --- |
| SEP | `dv/sep/` | Secure enclave system tests, UVM, cocotb, directed firmware |
| SMC | `dv/smc/` | SMC chiplet testbench, 1-core and 4-core management CPU variants |
| SMU | `dv/smu/` | Full SMU integration tests |
| DTP | `dv/dtp/` | Debug/test port and JTAG flows |
| Common VIP | `dv/common/`, `dv/vip/` | Shared UVM and cocotb VIP |

Firmware-directed tests exist under `fw/sep/tests/` for crypto, DMA, lifecycle,
watchdog, remap, eFuse, and related SEP functions. SMC firmware and test
sequences are under `fw/smc/`.

## Recommended Test Categories

```{mermaid}
flowchart TB
    Unit["Unit tests<br/>IP and component behavior"]
    Subsystem["Subsystem tests<br/>SEP, SMC, DTP"]
    Integration["SMU integration<br/>crossbar, resets, interrupts"]
    Firmware["Firmware tests<br/>BL0, SMC ROM, directed flows"]
    Security["Security validation<br/>lifecycle, debug gating, crypto policy"]
    Safety["FuSa validation<br/>fault reporting, reset, isolation"]

    Unit --> Subsystem
    Subsystem --> Integration
    Firmware --> Integration
    Security --> Integration
    Safety --> Integration
```

## High-Value Scenarios

- Cold boot through SMC fuse sense, SRAM repair, SRAM zeroization, and SEP
  release.
- SEP BL0 boot path with valid and invalid manifests.
- SMC local access, outbound access, and programmable address remap behavior.
- SEP-to-SMC, SMC-to-SEP, and external-SMN crossbar paths.
- Lifecycle-driven debug enable and disable behavior.
- JTAG-to-AXI access to permitted and denied regions.
- PLIC and SEP interrupt delivery, including watchdog paths.
- Reset isolation, functional-level reset, and JTAG reset override.
- Crypto accelerator access through AXI4-Lite/TL-UL bridge paths.
- Fault reporting into the external safety monitor interface.

## Build and Test Entry Points

The source README advertises root Makefile targets for subsystem tests such as
`make test-sep`, `make test-smc`, and `make test-smu`. Full execution depends
on repository initialization, third-party dependency checkout, generated CSR
collateral, and the local simulator/toolchain environment.

## Documentation Caveat

This site documents the architecture and implementation snapshot. It does not
claim that all listed verification scenarios have passed in the current local
workspace; simulation results should be attached as a separate regression report
when available.
