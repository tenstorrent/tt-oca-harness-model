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

Both blocks are covered by the single `sep_scratch_device` in `sep_scratch_cold.h`.
No separate model is needed for warm.
