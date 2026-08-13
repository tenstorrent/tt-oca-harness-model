# sep_scratch_warm — Model Plan

This block is structurally identical to `sep_scratch_cold`. See the combined plan:

**[sep_scratch_cold/sep_scratch_cold_plan.md](../sep_scratch_cold/sep_scratch_cold_plan.md)**

## Warm-specific parameters

| Parameter | Value |
|---|---|
| Base address | `0x1080_2080` |
| Size | `0x40` (8 × 8 B registers) |
| Reset behavior | Cleared on warm or cold reset (hardware only; VP: same as cold) |

## Key difference from cold

`sep_scratch_warm` has **no virtual console**. All 8 registers are plain R/W with no
side-effects. The virtual console protocol is attached only to `sep_scratch_cold.SCRATCH[2]`
at `0x1080_2010`.

An earlier draft of this plan proposed covering both blocks with a single
`sep_scratch_device` in `sep_scratch_cold.h`, and concluded no separate warm model was
needed. That is not what was built: cold and warm are separate modules,
`sep_scratch_cold_ip` and `sep_scratch_warm_ip`, each owning its own register bank and
`rst_ni`. Splitting them keeps the cold-domain side effects — the VP-ack shims and the
virtual-console and status decoders — out of a block that has none of them, and lets the
two sit in different reset domains, which is what the hardware actually does.
