# Key Manager specification

This directory holds the hardware team's own Key Manager documentation, vendored from
the `tt-oca-hw` `key_manager` block. It is the specification of record for the model:
where this and any other document disagree, this one is right.

| Path | Covers |
|---|---|
| [`KM_RTL_OVERVIEW.md`](KM_RTL_OVERVIEW.md) | Block overview, CPU-side memory map, reset and interrupt architecture, scrambling, execute whitelist. Start here. |
| [`doc/architecture.adoc`](doc/architecture.adoc) | Memory map, reset conditioning, KPV and its scrambling, wipe state, KMCSR, CRC accelerator, ROM code, parity and SRAM write locking, Adams Bridge sideload interfaces |
| [`doc/firmware.adoc`](doc/firmware.adoc) | Mailbox framing, the command and response set, return and fault codes, key provisioning and transfer |
| [`regs/`](regs/) | SystemRDL register sources, with the generated C headers under [`regs/gen/c/`](regs/gen/c/) |
| [`rtl/`](rtl/) | RTL sources, `key_manager.sv` down |
| [`dv/fw/`](dv/fw/) | The ROM firmware. `dv/fw/tests/` is the cocotb regression |

Two files are worth naming individually, because the model was written against them and
they settle most questions about intended behaviour: `dv/fw/drivers/rom_cmd.c` is the
authority on command validation order and return codes, and `regs/gen/c/km_mailbox_sep.h`
defines the only register block SEP can reach.

Nothing here is compiled by this repository. It is reference material.

## Deviations from the vendored tree

Two, both worth knowing if you re-sync this directory from `tt-oca-hw`:

- The block's own `README.md` is checked in as `KM_RTL_OVERVIEW.md`, so that this index
  can occupy `README.md`. Its content is unchanged.
- The generated register renderings other than C were dropped as redundant — the same
  eleven register blocks were also emitted as SystemVerilog, SV headers, Python, HTML,
  AsciiDoc and RAL, for build flows this repository does not run. The C headers were
  kept because the ROM firmware includes eleven of them.

## What used to be here

This directory previously held seven hand-written Markdown summaries of the hardware
spec (`KM_DESIGN_SPEC.md`, `KM_FW.md`, `KM_REGISTERS.md`, `KM_TEST_PLAN.md`,
`KM_COVERAGE_POINT.md`, `KeyManager.md`) and two block diagrams. They described the
earlier RTL revision — the one that exposed a second SEP-visible KPVLP window at
`0x10921000` and provisioned keys with `CMD_KPVLP_SLOT_REQ` and `CMD_KEY_REGISTER`.
None of that hardware exists any more. Restating the vendored spec in a second place is
what let those summaries drift out of date, so they were removed rather than rewritten.

## KM_GAP_ANALYSIS.md

[`KM_GAP_ANALYSIS.md`](KM_GAP_ANALYSIS.md) is kept, but read it for what it is: an RTL DV
coverage analysis, tracing the register description files and firmware spec against the
RTL verification test plan. It discusses AXI VIP access and UVM backdoor reads, so it
describes the RTL testbench rather than this model, and its body predates the rework.
The banner at the top is the part that still applies — it lists the behavioural deltas
between the two RTL revisions, and is the quickest summary of what changed in the model.

For the model's own design and test documentation, see
[`../02_key_manager_HighLevel_Design.md`](../02_key_manager_HighLevel_Design.md) and
[`../03_key_manager_Test_Plan.md`](../03_key_manager_Test_Plan.md).
