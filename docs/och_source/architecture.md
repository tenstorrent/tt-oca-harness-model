# OCH System Architecture

## Executive Summary

OCH, also referred to in the source tree as OCAH, is a reusable chiplet
management harness for OCA-style systems. Its purpose is to provide the
non-differentiating infrastructure that every chiplet needs: secure boot,
debug and test access, reset and power orchestration, platform firmware,
telemetry, lifecycle control, and attachment to the system-management network.

The current implementation is organized around the **System Management Unit
(SMU)**. The SMU integrates three peer subsystems:

- **SMC**: System Management Controller for platform orchestration.
- **SEP**: Secure Enclave Processor for root-of-trust and security services.
- **DTP**: Debug and Test Port for JTAG, iJTAG, boundary scan, and debug access.

The design intentionally separates reusable logic from adopter-specific
technology macros. PLLs, eFuse macros, SRAM and ROM macros, TRNG sources,
GPIO pads, SPI pads, padring wrappers, and vendor DFT cells are connected
through integration shims instead of being hard-coded into the reusable core.

## Top-Level Structure

```{mermaid}
flowchart TB
    Wrapper["smu_wrapper.sv<br/>Pads, macros, customer integration"]
    SMU["smu.sv<br/>Reusable OCH integration top"]
    SMCShim["smc_ip_integration.sv<br/>PLL, PVT, GPIO, eFuse, memory shims"]
    SEPShim["sep_ip_integration.sv<br/>SEP memory, SPI, TRNG shims"]

    DTP["DTP<br/>JTAG / iJTAG / cross-trigger"]
    SMC["SMC<br/>Rocket management CPU + fabric + peripherals"]
    SEP["SEP<br/>VeeR secure enclave + crypto"]
    XBAR["SMU AXI crossbar<br/>SEP, SMC, external SMN routing"]
    SMN["External SMN / chiplet fabric"]

    Wrapper --> SMU
    Wrapper --> SMCShim
    Wrapper --> SEPShim
    SMU --> DTP
    SMU --> SMC
    SMU --> SEP
    SMU --> XBAR
    XBAR <--> SMN
    SMCShim --> SMC
    SEPShim --> SEP
```

The integration top is `hw/smu/rtl/smu.sv`; there is no separate `hw/top`
directory in the inspected tree. `smu_wrapper.sv` is the adopter-facing wrapper
that demonstrates how macros and pads attach to the reusable SMU core.

## Subsystem Responsibilities

| Subsystem | Primary responsibility | Implementation focus |
| --- | --- | --- |
| SMU | Chiplet management hub | Composes SMC, SEP, DTP, and the SMU AXI crossbar |
| SMC | Platform orchestration | Boot control, reset, power, clocks, DMA, telemetry, peripheral management |
| SEP | Security root | Secure boot, lifecycle, crypto, attestation, key management |
| DTP | Debug and test | JTAG, iJTAG, STAP/PTAP topology, JTAG-to-AXI, cross-trigger |
| SMN | Management network | External system-management-network attachment; detailed SMN docs are still placeholders |
| AoU | AXI-over-UCIe bridge | Planned D2D path; current docs are placeholders |
| SMO | Safety monitor | External fault reaction agent for FuSa flows |

## Connectivity Model

```{mermaid}
flowchart LR
    SEPOut["SEP outbound AXI"]
    SMCOut["SMC outbound AXI"]
    ExtIn["External SMN inbound"]
    XBAR["SMU AXI crossbar<br/>3x3 non-reflexive routing"]
    SEPIn["SEP inbound AXI"]
    SMCIn["SMC inbound AXI"]
    ExtOut["External SMN outbound"]

    SEPOut --> XBAR
    SMCOut --> XBAR
    ExtIn --> XBAR
    XBAR --> SEPIn
    XBAR --> SMCIn
    XBAR --> ExtOut
```

The SMU crossbar provides non-reflexive connectivity between SEP, SMC, and
external SMN paths. SEP and SMC target apertures are runtime-programmable. The
external outbound window is a static catch-all region beginning at
`0x0000_8000_0000`.

## Addressing Model

OCH uses local subsystem views plus programmable global views.

| Region | Description |
| --- | --- |
| SMC local alias | Fixed `LOCAL_BASE = 0xC000_0000`, covering the SMC 32 MB local aperture |
| SMC global aperture | Programmed through `GLOBAL_BASE` and `REGION_SIZE` before outbound system traffic |
| SEP local space | 32-bit SEP map with local, SMC, SMU, AXI extension, and CPU TCM regions |
| SMU external window | Static external outbound region from `0x0000_8000_0000` |

The current source documentation has strong SMC and SEP memory maps, but the
top-level OCH memory map is still incomplete. A production integration should
close that gap before treating addresses as a final SoC contract.

## Clock and Reset Architecture

OCH has multiple functional clock domains: SMU/SMC/SEP core, always-on
reference, peripheral, telemetry, SEP watchdog, JTAG TCK, and asynchronous
entropy sampling. Reset is hierarchical: cold reset enters the SMC reset unit,
then domain-specific resets are synchronized and released in staged order.

```{mermaid}
sequenceDiagram
    participant Integrator
    participant SMC as SMC Reset Unit
    participant SEP as SEP
    participant DTP as DTP
    participant Subsystems as Managed Subsystems

    Integrator->>SMC: Assert and release rst_cold_ni
    SMC->>SMC: Sense fuses and sample straps
    SMC->>Subsystems: Request memory repair if enabled
    SMC->>Subsystems: Auto-zero SRAM if enabled
    SMC->>SMC: Start SMC firmware from ROM
    SMC->>SEP: Release SEP after fuse sense
    SMC->>Subsystems: Release reset and isolation per policy
    DTP-->>SMC: Optional JTAG reset override paths
```

## Security Architecture

The SEP is the security root. It contains a VeeR EL2 secure processor,
OpenTitan-derived crypto blocks, lifecycle control, key management, entropy
and DRBG infrastructure, DMA, watchdog, and secure boot firmware. Debug access
is lifecycle-gated. Secure boot follows a BL0-to-BL1 chain with public-key
verification and lifecycle-dependent policy.

Security mechanisms visible in the source and docs include:

- Dedicated lifecycle states and demotion controls.
- eFuse shadowing and feature-disable vectors.
- Traffic filters and address remapping at management-fabric boundaries.
- Crypto alert aggregation.
- Separate key bus for key-manager integration.
- Debug access conditioned on lifecycle and security straps.

## Functional Safety

OCH targets safety-oriented integrations where an external Safety Monitor
(SMO) reacts to fault signals. The documentation identifies safety mechanisms
such as ECC, parity, watchdogs, reset isolation, timeout handling, DCLS-style
CPU options, and per-subsystem fault reporting. The safety monitor is external
to the reusable harness.

## Architecture Gaps and Assumptions

The current OCH tree is active work. The published architecture should be read
as an implementation-oriented snapshot, not a frozen product specification.

- The top-level OCH memory map is referenced but not complete in the local docs.
- SMN and AoU chapters are placeholders.
- Several SEP interrupt and DMI paths are tied off or marked TODO in the SMU.
- `hw/periph/periph.sv` is a stub; peripherals are integrated through SMC
  wrappers and IP-integration paths.
- OCA compliance text is still under development.
