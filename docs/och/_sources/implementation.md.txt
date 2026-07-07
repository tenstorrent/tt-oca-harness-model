# Implementation Architecture

## Source Tree Orientation

The implementation lives under `sw/tt-oca-hw-main` in the local workspace.
The major implementation directories are:

| Path | Purpose |
| --- | --- |
| `hw/smu/rtl/` | SMU integration top, wrapper, and AXI crossbar |
| `hw/smc/` | SMC subsystem RTL, Rocket cluster wrapper, fabric, peripherals, reset unit |
| `hw/sep/` | SEP subsystem RTL, VeeR CPU, crypto, lifecycle, local fabric |
| `hw/dtp/` | Debug/test port RTL and JTAG/iJTAG infrastructure |
| `hw/ip/` | Reusable IP blocks such as crypto, JTAG, mailboxes, timers, and remappers |
| `hw/comp/` | Components with register-generation and integration collateral |
| `hw/periph/` | Vendor-macro wrappers and reference peripheral integration shims |
| `hw/common/` | Shared primitives, AXI/TL-UL glue, assertions, and FuSa helpers |
| `meta/registers/` | Register descriptions used as CSR single source of truth |
| `fw/sep/` | SEP boot ROM and directed firmware tests |
| `fw/smc/` | SMC firmware, ROM, common code, and test sequences |
| `dv/` | SEP, SMC, SMU, DTP, and shared verification environments |

## SMU Implementation

`smu.sv` is the reusable integration top. It instantiates the DTP, SMC, SEP
when enabled, and the SMU AXI crossbar. `smu_wrapper.sv` is the integration
wrapper that adds pad/macro-level connections and demonstrates adopter-facing
wiring.

The SMU crossbar is a three-master/three-slave fabric:

- SEP outbound AXI.
- SMC outbound AXI.
- External SMN inbound AXI.
- SEP inbound AXI.
- SMC inbound AXI.
- External SMN outbound AXI.

The crossbar deliberately avoids reflexive routes. SEP-to-SEP and SMC-to-SMC
traffic stay local to their subsystems; crossbar traffic is for peer or external
access.

## SMC Implementation

The SMC is the platform-management controller. It combines a RISC-V management
CPU, management fabric, reset and power control, data movement engines,
peripherals, and debug hooks.

```{mermaid}
flowchart TB
    CPU["Rocket RV64GC cluster<br/>1 or 4 cores"]
    Fabric["SMC fabric<br/>input, local, output"]
    Periphs["SMC peripherals<br/>GPIO, UART, I2C, AVSBus, PVT, mailbox"]
    Reset["SMC reset unit<br/>domain release, isolation, FLR"]
    DMA["DMA and zeroer"]
    Debug["JTAG / DFD / BEU"]
    SEPIn["SEP inbound path"]
    SysOut["System outbound AXI"]

    CPU <--> Fabric
    Fabric <--> Periphs
    Fabric <--> DMA
    Fabric <--> Debug
    Fabric <--> SEPIn
    Fabric --> SysOut
    Reset --> CPU
    Reset --> Periphs
    Reset --> DMA
```

Key implementation points:

- Rocket RV64GC is generated through the SMC CPU flow and uses TileLink
  internally, with AXI-facing boundaries.
- The SMC fabric bridges CPU, JTAG debug, SEP inbound, system inbound/outbound,
  local peripheral, remap, and filter paths.
- The local aperture is 32 MB at `0xC000_0000`.
- The PLIC supports a large interrupt fan-in for multi-core SMC operation.
- Reset orchestration exports subsystem reset and isolation controls.
- I3C and some external integration points are reserved or shimmed depending on
  the build.

## SEP Implementation

The SEP is the root-of-trust subsystem. It contains a VeeR EL2 core, local
AXI fabric, secure boot ROM, lifecycle controller, cryptographic accelerators,
watchdog, DMA, secure memories, and IO wrappers.

```{mermaid}
flowchart TB
    VeeR["VeeR EL2<br/>RV32 secure CPU"]
    LocalFabric["SEP local AXI fabric"]
    Crypto["Crypto complex<br/>AES, HMAC, KMAC, OTBN, KM, entropy"]
    LCC["Lifecycle controller"]
    ROM["BL0 ROM and TCMs"]
    DMA["SEP DMA"]
    IO["SPI / low-speed IO"]
    SMUAXI["SMU / SMC / external AXI paths"]

    VeeR <--> LocalFabric
    LocalFabric <--> Crypto
    LocalFabric <--> LCC
    LocalFabric <--> ROM
    LocalFabric <--> DMA
    LocalFabric <--> IO
    LocalFabric <--> SMUAXI
```

OpenTitan-derived blocks retain TL-UL internals in several places and are
bridged to AXI4-Lite through protocol adapters. The key manager uses a
dedicated key bus instead of side-load wiring. Memories and entropy sources are
integrated through wrappers so an adopter can bind real macros and technology
IP outside the reusable core.

## DTP Implementation

The DTP provides JTAG, iJTAG, scan, debug, JTAG-to-AXI, and cross-trigger
connectivity. The documented scan topology is:

```text
PTAP -> I/O STAP -> SMC debug STAP -> SEP debug STAP -> extra STAPs -> PTAP
```

Lifecycle state gates access to debug chains. SMC debug depends on SoC or
application debug authorization; SEP debug additionally depends on SEP debug
authorization. Cross-trigger paths connect SMC, SEP, and external trigger
ports, with some DTP-internal trigger ports reserved for SMC synchronization.

## Bus and Protocol Stack

| Protocol | Use |
| --- | --- |
| AXI4 | SMU crossbar, SMC/SEP system ports, CPU front ports, DMA |
| AXI4-Lite | CSRs, peripherals, debug and remap/filter control |
| TileLink / TL-UL | Rocket internals and OpenTitan-derived IP internals |
| APB4 | Selected DFD, watchdog, timer, and GPIO pad-domain blocks |
| AXI-Stream | Entropy/TRNG input paths |
| ATB | Telemetry receiver streaming |
| JTAG / iJTAG | DTP, STAP/PTAP chains, JTAG-to-AXI |

The register-generation flow treats SystemRDL as the CSR source of truth, with
generated RTL, SystemVerilog headers, and C headers produced by the local tools.

## Firmware

The firmware tree contains both secure and management firmware:

- SEP BL0 ROM code, manifest parsing, crypto helpers, lifecycle handling, and
  directed tests under `fw/sep/`.
- SMC ROM, Rust application work, shared firmware code, and test sequences under
  `fw/smc/`.

Firmware is responsible for early base-address programming, lifecycle-aware
policy, inter-subsystem access setup, and secure or management service bring-up.

## Current Implementation Caveats

Several implementation areas are visibly work-in-progress:

- SMN and AoU are architectural placeholders.
- Some SEP timer, soft interrupt, external interrupt, and DMI paths are tied off
  or marked TODO at the SMU boundary.
- The standalone `hw/periph/periph.sv` top is a stub.
- Generated or third-party dependency outputs may require repository
  initialization before every RTL path can be elaborated.
