# lc_ctrl — Lifecycle Controller Model Summary

## Overview

`lc_ctrl` is a SystemC TLM-2.0 model of the SEP Lifecycle Controller, mirroring
`hw/sep/sep_lifecycle_ctrl.sv`. Its job is narrow: given the lifecycle state and the
fuse-programmed disable masks, compute the 64-bit `FEAT_CTRL` vector that gates the chip's
debug, test and functional features, and let firmware demote into PROD_DBG.

The register window is `0x1091_8000`, size `0x18`, per
`meta/registers/rdl/sep_lifecycle_ctrl.rdl`.

## Register Map

| Register | Offset | Width | SW Access | Reset | Description |
|---|---|---|---|---|---|
| `FEAT_CTRL_LO` | `0x0000` | 32 | RO | `0x0` | Feature vector `[31:0]` — hardware-written |
| `FEAT_CTRL_HI` | `0x0004` | 32 | RO | `0x0` | Feature vector `[63:32]` |
| `DEMOTE_1` | `0x0008` | 32 | RW (W1S) | `0x0` | `demote[0]`, `lock[1]`, `rsvd[31:2]` — written by BL1 |
| `DEMOTE_1_HI` | `0x000C` | 32 | RW (W1S) | `0x0` | `rsvd[63:32]` |
| `DEMOTE_2` | `0x0010` | 32 | RW (W1S) | `0x0` | Same layout, written by BL2 |
| `DEMOTE_2_HI` | `0x0014` | 32 | RW (W1S) | `0x0` | `rsvd[63:32]` |

`FEAT_CTRL` and `DEMOTE` are both `regwidth = 64` in the RDL; the model splits each into
two 32-bit words because the bus is 32-bit.

The feature vector has three sections, matching the generated
`sep_efuse_map_lc_disable_reg_t` struct: debug `[31:0]`, test `[47:32]`, func `[63:48]`.

## Inputs

The block has no lifecycle state of its own, and neither does the RTL:
`sep_lifecycle_ctrl.sv` is combinational on `shadow_regs_i`, `security_disable_i` and
`secure_tm_i`, all from the eFuse wrapper. The model takes the same bundle through
`set_inputs()` and recomputes `FEAT_CTRL` on each call.

| Input | Source in silicon |
|---|---|
| `lc_state_code` | eFuse `LC_STATE`, as the 8-bit differential code `{~raw, raw}` |
| `sip_dis` / `sys_dis` | eFuse `SiP_DIS` / `SYS_DIS` — `woset`, so software-writable at runtime |
| `security_disable` | eFuse `security_disable_o`: SEC_DISABLE token match, gated by the silicon revision |
| `secure_tm` | The `test_en` strap, latched by `sep_efuse_wrapper.sv` at fuse-sense-done |

On a platform, `och_sep_ss` drives all of these from the eFuse model and re-applies them
whenever the eFuse shadow registers change, so a `woset` write to `SiP_DIS` moves
`FEAT_CTRL` the way it does in silicon. The CCI parameters only seed the bundle, for a
standalone testbench that has no eFuse to ask.

## State to Feature Vector

| State | Encoding | `FEAT_CTRL` |
|---|---|---|
| `TEST_DEV` | `4'b0000` | `~(sip_dis \| sys_dis)`; a demote re-enables the whole debug section |
| `PROD` | `4'b0001` | Func section only. `demote_1` → PROD_DBG_1: full debug plus `~sip_dis.func`. `demote_2` → PROD_DBG_2: full debug plus `~sys_dis.func` |
| `PROD_END` | `4'b1000` | Func section only, as the PROD base |
| `RMA_SIP` | `4'b001x` | `~sip_dis` |
| `RMA_CHIPLET` | `4'b011x` | All ones |
| anything else | — | Zero: every feature disabled |

`demote_1` takes priority over `demote_2`. Two overrides are applied after this chain, in
order: `security_disable` forces all ones, then `secure_tm` deasserted clears the test
section. Because `security_disable` comes after the chain, it also overrides the fail-safe
below — a part with security disabled reads all ones even with a corrupted LC code.

## Differential Encoding

Two interfaces carry dual-rail values, and both matter for fidelity:

- **`LC_STATE` in.** The RTL decodes `lc_state[7:0]` as `{diff_n, diff_p}` through
  `prim_diff_decode_multi`. A pair that is not `{~raw, raw}` raises `lc_sigint_err_o`, which
  forces `feat_ctrl = 0`. That is the block's fail-safe against a tampered lifecycle state,
  and it is distinct from a legally encoded but unassigned state: both zero the vector, but
  only the former reports an integrity error. `get_lc_sigint_err()` exposes it.
- **Demote state out.** `sep_crypto.sv` drives the key manager's `OTP_DEMOTION_STATE` from
  `prim_diff_encode_multi`, so each 2-bit field is `{~v, v}`. `get_demote_state()` returns
  `demote_1` in `[1:0]` and `demote_2` in `[3:2]`, which makes `0xA` — not `0x0` — the
  undemoted value.

## DEMOTE Semantics

Every field is `onwrite = woset`: bits only ever set, and a reset is the only thing that
clears them. `swwe = ~lock` applies to the `demote` bit alone — `lock` and `rsvd` carry no
`swwe` and stay writable once locked, though rewriting a `woset` lock is a no-op. Any
accepted write recomputes `FEAT_CTRL`.

## Design Points

- **No `SC_THREAD` or `SC_METHOD`.** The model is combinational, like the RTL: `FEAT_CTRL`
  is recomputed synchronously whenever an input or a demote register changes.
- **`FEAT_CTRL` is read-only to software.** Its write mask is `0x0`; the model writes it
  directly, bypassing the mask, because in hardware it is hardware-driven.
- **Not modelled yet.** `feat_ctrl.sep_debug` reaches the inbound filter's skip input in
  `sep.sv` and `prod_dbg_active` feeds back into the eFuse to freeze lifecycle transitions.
  Neither loop exists in the VP; both are tracked as action items L2 and L3 in the
  lifecycle RTL-versus-VP comparison.

## Tests

Tests 1-12 read `FEAT_CTRL` as the configuration leaves it, so each covers a single state
arm and skips otherwise; the combos in `config/accellera_config.ini` select between them.
Tests 13-19 drive the input bundle directly and cover every state arm, the `lc_sigint_err`
fail-safe, the live feature-disable path, the demote encoding, the upper `DEMOTE` words, the
lock scope and the PROD_DBG priority in one run.

Run them with `./run_tests.sh`; see the [README](README.md) for the build options.
