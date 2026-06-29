# 01 — sep_status_report Specification

## Purpose

Make the SEP bootcode's **production** status stream visible during a virtual-platform
run. On silicon the SEP firmware pushes encoded status codes into a status ring buffer in
SMC SRAM, which the SMC reads to report authoritative boot progress and failures. In the
VP that ring lives in the `smc_global` functional stub; this component observes the
firmware's writes into it and renders each status code as a human-readable `SEP_STATUS`
line through the shared CSML logger.

It is the structured-status peer of `sep_virt_console` (which surfaces the free-form
`simput*` debug path as `SIM_OUT`). The two coexist and are independent.

## Scope

- **In scope**: observe ring `entries[]` writes; decode `type`/`fw_id`/`value`; resolve
  the value to a symbolic name from a run-time TSV; emit one CSML line per code, in the
  order written; enable/disable and verbosity via CCI.
- **Out of scope**: modeling the SMC consumer that drains the ring (advances `tail`) — a
  future SMC-emulation model owns that; the `SIM_OUT` debug path; the secondary debug
  post-code register (`COLD_SCRATCH_1`).

## Functional behavior

1. Every non-debug status code the firmware writes to the ring `entries[]` region is
   surfaced as exactly one line, in emission order.
2. Each line shows the firmware stage (`BL0`/`BL1`/`ID%u`), severity
   (`INFO`/`WARN`/`ERROR`/`INFO_EXT`/`T0x%02x`), the value (`0x%04x`), and the symbolic
   name (or `SEP_MSG_UNKNOWN`).
3. Names come from a value→name TSV loaded at run time (no compiled table). A
   missing/unreadable/empty/malformed TSV is reported once and degrades gracefully to
   `SEP_MSG_UNKNOWN`; the run never aborts.
4. Observation is non-invasive: the ring's read/write behavior is unchanged and `tail` is
   never advanced. With output disabled, no line is produced and no tap is installed.
5. Output is deterministic for a given firmware image, configuration, and TSV.

## Interfaces

- **Input**: a write-tap callback `(offset, data, len)` from the `smc_global` `SEPMemory`
  instance, filtered by the platform to the ring `entries[]` window; the component
  assembles a little-endian 32-bit word (sub-word/misaligned writes ignored).
- **Output**: CSML log lines with a literal `SEP_STATUS` source field.
- **Configuration (CCI)**: `enable` (bool, default true), `verbosity` (int, default 2),
  `names_tsv` (string path).

## Traceability

- Word encoding: `fw/sep/bootcode/include/errors.h` (`STATUS_ENCODE`, type constants).
- Ring protocol: `fw/sep/bootcode/src/status_ring.c`.
- Canonical names: `meta/status/status_values.tsv`.
