# Roadmap and Open Items

## Current Status

The OCH source tree is a working implementation snapshot. SMC, SEP, DTP, SMU
integration, memory maps, methodology, FuSa, and several IP blocks have useful
source documentation. Some top-level architectural chapters and future
subsystems are still placeholders.

## Open Architecture Items

| Area | Status | Recommended next action |
| --- | --- | --- |
| Top-level OCH memory map | Referenced but incomplete | Author a consolidated SMU/SMC/SEP/SMN memory map |
| SMN | Placeholder | Define topology, routing, QoS, protection, and integration ports |
| AoU | Placeholder | Document AXI-over-UCIe bridge architecture and D2D flows |
| OCA compliance | Under development | Map OCH modules to OCA requirements and terminology |
| Periph top | Stub | Clarify whether `hw/periph/periph.sv` remains reference-only |
| SEP interrupt/DMI tie-offs | WIP | Close TODOs or document unsupported modes |
| Verification results | Environment-dependent | Publish regression logs and pass/fail matrix |

## Suggested Publication Cadence

1. Publish the current architecture and implementation snapshot.
2. Add generated API/reference pages for stable register maps.
3. Add subsystem-specific deep dives for SMC, SEP, DTP, and SMU.
4. Add verified boot logs, smoke-test results, and waveform-backed diagrams.
5. Fold in SMN and AoU details once those implementations move beyond
   placeholders.

## Source References

The first version of this documentation was derived from local files under:

- `sw/tt-oca-hw-main/doc/`
- `sw/tt-oca-hw-main/hw/smc/doc/`
- `sw/tt-oca-hw-main/hw/sep/doc/`
- `sw/tt-oca-hw-main/hw/dtp/doc/`
- `sw/tt-oca-hw-main/hw/smu/doc/`
- `sw/tt-oca-hw-main/hw/smu/rtl/`
- `sw/tt-oca-hw-main/hw/smc/`
- `sw/tt-oca-hw-main/hw/sep/`
- `sw/tt-oca-hw-main/fw/`
- `sw/tt-oca-hw-main/dv/`
