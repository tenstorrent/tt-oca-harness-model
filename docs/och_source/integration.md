# Integration Guide

## Integration Philosophy

OCH separates reusable management logic from adopter-specific technology
choices. The reusable RTL should not directly instantiate foundry SRAMs, ROMs,
PLLs, eFuse macros, padrings, TRNG cells, or vendor DFT logic. Those are bound
through wrapper and shim layers.

The integration pattern is:

1. Instantiate `smu_wrapper.sv` at the chiplet level.
2. Connect adopter pads, resets, clocks, straps, and lifecycle inputs.
3. Bind vendor macros through `smc_ip_integration.sv` and
   `sep_ip_integration.sv`.
4. Program address apertures before issuing cross-subsystem or external AXI.
5. Release managed subsystem resets and isolation through SMC policy.

## Wrapper Boundary

```{mermaid}
flowchart LR
    SoC["Chiplet / adopter SoC"]
    Pads["Pads and macro wrappers"]
    SMUWrapper["smu_wrapper.sv"]
    SMU["smu.sv reusable core"]
    SMCShim["SMC IP integration"]
    SEPShim["SEP IP integration"]
    System["System AXI / SMN / D2D"]

    SoC --> Pads
    Pads --> SMUWrapper
    SMUWrapper --> SMU
    SMUWrapper --> SMCShim
    SMUWrapper --> SEPShim
    SMU <--> System
```

This boundary keeps reusable OCH logic portable while allowing chiplet programs
to bind their own SRAMs, ROMs, eFuses, clocking, pad libraries, and DFT cells.

## Bring-Up Sequence

```{mermaid}
sequenceDiagram
    participant Board as Board / Integrator
    participant SMU as SMU Wrapper
    participant SMC as SMC Firmware
    participant SEP as SEP Firmware
    participant System as External System

    Board->>SMU: Provide stable power and clocks
    Board->>SMU: Assert then release cold reset
    SMU->>SMC: Start reset sequencing
    SMC->>SMC: Sense eFuses and sample straps
    SMC->>System: Optionally request memory repair
    SMC->>SMC: Auto-zero SRAM unless disabled
    SMC->>SMC: Program GLOBAL_BASE and REGION_SIZE
    SMC->>SEP: Release SEP after fuse sense
    SEP->>SEP: Execute BL0 secure boot policy
    SMC->>System: Release subsystem resets and isolation
```

## Required Early Firmware Actions

| Action | Owner | Why it matters |
| --- | --- | --- |
| Program SMC `GLOBAL_BASE` and `REGION_SIZE` | SMC firmware | Enables correct outbound system routing |
| Program SEP global base and region CSRs | SEP or platform firmware | Enables SEP cross-subsystem and external accesses |
| Confirm SMU aperture non-overlap | Platform firmware / integration | Prevents ambiguous crossbar decode |
| Apply lifecycle-derived debug policy | SEP / SMC / DTP | Gates JTAG, iJTAG, and debug chains |
| Release reset and isolation in order | SMC firmware | Prevents traffic into uninitialized domains |

## Address Integration

SMC local software uses a fixed local alias at `0xC000_0000`. Platform software
must program the global base and region size so outbound traffic reaches the
expected chiplet address range. SEP has a separate 32-bit view with local,
SMC, SMU, external, AXI-extension, and TCM regions.

The SMU crossbar adds the peer and external routing layer. Integrators should
reserve non-overlapping regions for SEP, SMC, and external traffic and keep the
static external outbound region clear of subsystem apertures.

## Clock and Reset Integration

Clock domains visible at the OCH boundary include:

| Clock | Domain |
| --- | --- |
| `clk_smu_i` / `clk_smc_i` | SMU, SMC, SEP core logic |
| `clk_ref_i` | Always-on reference logic and reset synchronization |
| `clk_periph_i` | Peripheral domain |
| `clk_telemetry_i` | Telemetry / ATB paths |
| `clk_sep_wdt_i` | Independent SEP watchdog |
| `jtag_tck` | DTP and JTAG scan chains |
| `entropy_rosc_sample_clk_i` | SEP entropy sampling |

Reset integration should treat cold reset as the root event. Warm, watchdog,
functional-level reset, JTAG override, and subsystem reset requests are then
resolved through the SMC and SEP reset-control logic.

## Debug and Test Integration

DTP is the chiplet debug/test ingress. It provides primary TAP, secondary TAPs,
iJTAG chains, JTAG-to-AXI access, and cross-trigger connections. Debug access
must be lifecycle-aware. For production integrations, the scan-chain topology
and debug unlock policy should be reviewed with security owners before tapeout.

## Integration Checklist

- Confirm the target repository has initialized third-party dependencies and
  generated register collateral.
- Connect all required clocks and resets, including watchdog and entropy clocks.
- Bind adopter SRAM, ROM, eFuse, PLL, pad, TRNG, and DFT macros through shims.
- Program SMC and SEP global apertures before external AXI traffic.
- Validate lifecycle/debug policy for every JTAG and iJTAG access path.
- Confirm PLIC, SEP PIC, watchdog, and fault outputs are connected.
- Review SMN and AoU placeholders before relying on D2D or management-NoC
  behavior.
- Treat the top-level OCH memory map as an integration deliverable until the
  missing source document is completed.
