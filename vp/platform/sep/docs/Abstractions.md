# SEP VP — Known Abstractions, Simplifications & Deferred Gaps

This document tracks every place the SEP SystemC/TLM virtual platform (`vp/platform/sep/`,
`sep/peripherals/*`) knowingly diverges from real RTL — either because TLM-2.0's
`tlm_generic_payload` has no field to carry the information, or because a structural
simplification produces identical observable behavior at LT (loosely-timed) abstraction with
much less code. Each entry states *what* is simplified, *why*, and *when it would actually
matter* (i.e. what test/scenario would first expose the divergence).

Bugs that were found and fixed are not listed here — this file is only for gaps that remain
today, by design or by deferral.

---

## 1. TLM-inherent limitations (no field exists on `tlm_generic_payload` to carry this)

These are permanent unless a custom TLM extension is added to carry the missing AXI sideband
signal end-to-end. None of the current firmware tests exercise the security properties these
would enforce, but a hardware-equivalence test written specifically to probe them would fail.

`sep/utils/tlm_extensions/sep_axi_extension.h` now carries `source_id` and `is_ns`, so the two
entries that depended on those being unavailable (1.1 and 1.2) are resolved and recorded below
as such. They are kept in place, rather than deleted and the list renumbered, because 1.4 refers
back to 1.2.

### 1.1 `sep_filter_ctrl` — NS (secure/non-secure) — RESOLVED
`allow_ns` is now enforced. `match_entry()` requires `sep_axi_extension::is_ns` to equal the
entry's `allow_ns` exactly, matching `traffic_filter.sv`'s `pass_ns` — it is a level selector,
not a permission bit, so an entry admits one security level and not the other. This closes the
property **"the inbound traffic filter ensures only STEE has access to the STEE address
remapper"** (OCAH spec, SEP chapter), provided the initiator attaches the extension. An
unextended payload falls back to the extension's own defaults (`SEP_SOURCE_ID`, secure), which
keeps pre-extension models behaving as before.

The VeeR initiator stamps `is_ns = false` unconditionally, and that is not a simplification:
VeeR EL2 hardwires the port the filter reads, driving `lsu`/`sb` `AxPROT = 3'b001` and `ifu`
`ARPROT = 3'b101`, so bit 1 — the NS bit sampled at `axi_filter_wrap.sv:163,190` — is 0 on every
CPU port regardless of privilege mode. Do not "improve" this by deriving it from the ISS's
`privilegeMode()`: it would diverge from RTL, and since `pass_ns` is an exact match under
`BlockByDefault=1`, any access reporting non-secure would be denied by every `allow_ns=0` entry.
No SEP-side block sets the bit either — `axi_alias_remap`, `axi_window_remap` and the SPI/UART
wrappers all pass `prot` through untouched. The residual, which the VP cannot resolve on its own,
is what real STEE/SMC traffic drives on the inbound path; `adapters.h` currently stamps those with
`SMC_SOURCE_ID` and the secure default.

### 1.2 `sep_filter_ctrl` — `src_id` RESOLVED; `group_id` not enforced (and neither is it in RTL)
`src_id` is now enforced by `match_entry()`, with a zero CSR field acting as the wildcard, so an
entry naming no source admits every master. `group_id` remains unchecked, but this is not a
divergence: SEP wires `EnGroupIdFilter=0` on both filter instances
(`sep_system_peripherals.sv:345,392`), which holds `pass_group_id` at 1 in hardware too. The
field stays CSR-programmable so firmware can configure it against a future build that enables
the feature.

### 1.3 `sep_filter_ctrl` — burst-length approximated by byte count
Real RTL's `pass_burst` gates on `tx_len_i` (AXI burst *beat count*). `tlm_generic_payload` has
no beat-count concept, so the model approximates via `data_length > 8 bytes` (total transaction
bytes) instead. Functionally equivalent for single-beat vs. multi-beat classification in
practice, but not a literal match for an AXI burst with an unusual `size`/`len` combination.

### 1.4 `sep_output_remap_ctrl` — AXI user-field retagging not modeled
Real `output_remap.sv` forces `aw.user`/`w.user`/`ar.user` to a fixed `UserOverrideVal`
(`OTHERS_SOURCE_ID`) on every remapped transaction. No AXI `user` field exists on
`tlm_generic_payload`, and nothing downstream in the VP currently inspects a user/source-ID
sideband to make a functional decision — same underlying limitation as 1.2.

### 1.5 `sep_output_remap_ctrl` / general CSR layer — byte-enable not honored
The RDL declares `accesswidth=64` and real hardware's generated regblock additionally merges
partial (sub-64-bit) writes using `wstrb`-derived bit-enables. `regmodel::Memory`'s write path always
applies a full 64-bit overwrite regardless of the TLM byte-enable mask. Shared limitation of the
`regmodel` register library, not specific to one peripheral. No firmware access pattern found today
uses anything but full 64-bit reads/writes to these registers, so no observed practical impact.

### 1.6 `local_master_alias_remap_ctrl` — `cacheable` override carried but not consumed
The 16-region table's `cacheable` field is now applied on a region hit, overriding
`sep_axi_extension::cacheable` on both the functional and debug paths and passing the incoming
value through on a miss — matching `axi_alias_remap.sv:122,147`. What remains abstracted is the
other end: no VP target acts on the resulting attribute, because there are no caches at LT to
allocate into. The four `AxCACHE` bits are also collapsed to a single `bool`, which is lossless
for this IP (its RTL replicates one region bit across all four) but would need widening for a
master driving the finer-grained encodings. Both would matter only for an AT (approximately-timed)
model or a cache-coherency-sensitive test.

---

## 2. Deliberate structural simplifications (flat bus vs. real multi-stage fabric)

These produce **identical observable behavior** in the common/expected case, so they're treated
as free simplifications — not gaps — except where explicitly noted.

### 2.1 One flat `SimpleBus` instead of two real crossbars
Real hardware has an outer crossbar (`sep_local_axi_xbar_wrapper`, routing CPU/DMA to SRAM/TCM/
crypto/DMA-CSR/IO/WDT/"system peripherals"/AXI-extension) and, for the "system peripherals"
subset specifically, a second inner crossbar (`sep_system_peripherals_xbar_wrapper`, reached
after the demux's LOCAL leg, fanning out to mailbox/system CSR/etc.). Neither crossbar
transforms addresses — both are pure static routers — so a single flat `SimpleBus` with the
right `PortMapping` table produces identical results. No functional difference; not worth
building two literal bus classes.

### 2.2 One shared fixed local-alias-remap adapter instead of one per master channel
Real RTL has 3 separate `axi_local_alias_remap` instances in `sep_cpu.sv` (one per CPU AXI
channel: IFU/LSU/DBG) plus 1 in `sep_dma_wrap.sv`. The VP uses a single shared
`local_alias_remap_adapter` (`local_alias_fixed_adapter`) for all of them. The remap math
(`SEP_LOCAL_BASE_ADDR`/`SEP_REGION_SIZE`-based subtract-if-in-range) is identical regardless of
which channel issues the transaction, and SystemC TLM-2.0 LT processes transactions
sequentially anyway — no observable difference from combining them.

### 2.3 Bus re-injection stands in for the real `u_axi_demux`
`local_alias_remap_ip`'s `remapped_socket` re-injects its output as a new bus initiator, and the
bus's own static `PortMapping` table decodes the address a second time. Since Local, AP, and
STEE are each simply non-overlapping address windows with their own bus targets, this
"re-decode on the same flat bus" trick is a correct, if implicit, stand-in for the real 5-way
`u_axi_demux` (SMC/SMU/AP/STEE/Local) for those three legs.

### 2.4 SMU leg — now wired correctly as a 3rd input to `outbound_filter_mux` (fixed this session)
Real RTL's `u_outbound_filter_mux` (`sep_system_peripherals.sv`) takes exactly three
`slv_reqs_i`: AP-remapped traffic, STEE-remapped traffic, and the **raw** `SEP_EXT_TO_SMU` demux
output (no remap stage of its own) — all three merge before the shared Outbound Filter's
`BlockByDefault`/policy enforcement. The VP's `outbound_filter_mux` (`adapters.h`) now has three
target sockets (`ap_tgt`, `stee_tgt`, `smu_tgt`) to match; the SMU bus window forwards directly
into `smu_tgt` instead of terminating in a standalone stub. (Previously this was modeled as a
dead-end `smu_global_stub` that silently swallowed SMU-bound traffic before it ever reached the
Outbound Filter — a real functional bug, not just a structural simplification. Corrected.)

---

## 3. Deferred / open gaps (documented, not yet fixed)

### 3.1 System-peripherals-bound addresses bypass `local_alias_remap_ip`
Real hardware routes **all** traffic bound for mailbox, system CSR, the local-alias-remap/AP-
remap/STEE-remap/filter CSR blocks, reset_ctrl, and cpu_ctrl through `u_local_master_remap_wrap`
(the programmable 16-region remap) before the demux — even though in the common case (no active
alias-remap region overlapping those addresses) the address passes through unchanged. The VP
currently wires those as independent, directly-reachable bus targets from the CPU's raw
initiator socket, never touching `local_alias_remap_ip` at all.

**Impact:** identical to real hardware *unless* firmware configures one of the 16 alias-remap
regions to overlap the system-peripherals address range (`~0x10A0_0000`–`0x11FF_FFFF`), in which
case real silicon would additively remap that access and the VP would not.

**Why deferred:** fixing this properly requires making the fixed local-alias remap a mandatory
pre-stage for *every* CPU/DMA transaction (not just ones addressed into `[0xC0000000,
0xFFFFFFFF]`), which means restructuring how `riscv`/`dma`'s initiator sockets connect to the
bus — a materially bigger, riskier change than any single-peripheral fix made so far. Also
requires reading `sep_local_axi_xbar_wrapper`'s real address-decode table to get the exact
system-peripherals boundary right (not yet done — the boundary used above is inferred from the
OCAH memory-map doc, not RTL-confirmed).

### 3.2 CPU per-channel AXI connectivity restrictions not modeled
Per the OCAH spec's fabric connectivity matrix (§5.3.1), real hardware physically restricts
which AXI channel can reach which slave — e.g. the CPU instruction-fetch port (IFI) is **not
connected at all** to CSRs, System I/F, Crypto, or Peripheral IO, only to Boot ROM/Scratch
SRAM/ICCM. `VeeRISSTlm` exposes one unified initiator socket for the whole CPU, so this
per-channel restriction isn't modeled — anything reachable by the CPU at all is reachable via
any access type in the VP.

### 3.3 DMA's local-alias fixed remap doesn't model its SRAM carve-out
The DMA-side `axi_local_alias_remap` instance in real RTL has an SRAM-adjusted window distinct
from the CPU-side instances' raw `SEP_LOCAL_BASE_ADDR`/`SEP_REGION_SIZE`. Not modeled — the VP's
shared fixed adapter uses the same window logic for both CPU and DMA traffic.

### 3.4 `dma_alert`/`wdt_alert` PIC wiring — ambiguous at the RTL level itself
`sep.sv` references `dma_alert_o`/`wdt_alert_o`-style ports that don't exist on the *current*
`sep_dma_wrap.sv`/`sep_wdt_wrap.sv` (DMA has a differential `alert_tx_t` bus instead; WDT's
underlying `aon_timer`'s alert is explicitly tied off `/* UNUSED */`). This looks like a
real RTL inconsistency, not just a VP gap — left unwired pending hardware-team clarification.

### 3.5 SEP↔SMC/SMU integration boundary — partially exposed
The AXI boundary now exists as public sockets on `och_sep_ss`, so `smu-vp` binds the SEP as a
real participant rather than reaching into it:
- `sep_smn_inbound_axi` (RTL `smn_inbound_axi`) — inbound from the SMU crossbar, feeding the
  RTL-ordered chain `inbound_filter` → global→local window remap → internal bus.
- `sep_smn_outbound_axi` (RTL `smn_outbound_axi`) — everything the outbound filter passes,
  previously swallowed by a private `filter_output_stub`, so SEP egress was unobservable.
- `sep_ext_to_smc_axi` — the dedicated SMC-window master path.
- `sep_global_base_addr_o`/`sep_region_size_o` (`sc_signal` members driven by `cpu_ctrl` from its
  CSRs, mirroring `sep_system_csr.sv`) — the crossbar sizes its SEP aperture from these, so the
  window cannot be configured on one side and not the other.

Standalone `sep-vp` binds an idle initiator to the inbound port and sinks to the two outbound
ports, since there is no chiplet fabric above it.

What is still missing:
- `cpu_ctrl->hwif_in.smc_fuse_sense_done`/`sep_fuse_sense_done` are hardcoded `true` at
  construction rather than driven by real cross-module signals.
- `smc_global_base_addr_i`/`smc_region_size_i` (real RTL inputs from SMC that gate the
  `SEP_EXT_TO_SMC` demux window) aren't modeled as live signals — `smc_global`'s window is a
  static `Args.hpp` constant instead.
- No top-level ports exist for `lc_state_o`, `feat_ctrl_o`, `security_disable_o`,
  `lcc_demote_state`, `wdt_rst_ni`/`wdt_timer_rst_req_o` — all internal-only.
- The SMN inbound chain has no dedicated unit test. An earlier standalone testbench was removed
  because nothing ran it (`ci.yml`/`ci-rhel8.yml` build and run `sep-vp`, never `ctest`), and a
  test nobody runs isn't coverage. It is now exercised for real by the `smu-vp` link test instead.

### 3.6 Address-map entries pending RTL confirmation (found via OCAH doc cross-check, not yet verified against real RTL)
- UART/SPI look swapped relative to the OCAH memory map: doc places UART (shared with GPIO) at
  `0x10B0_0000`–`0x10BF_FFFF`; the VP has UART at a placeholder `0x44000000` (explicitly marked
  as such in `Args.hpp`) and SPI at `0x10B00000` instead. The doc also states SPI's base address
  is CSR-configurable in real hardware, not fixed. **Still unconfirmed** — but a related finding
  from a later session (see the `sep/peripherals` inventory audit below): `uart_16550` isn't a
  SEP peripheral at all in real RTL (`sep.sv` has zero UART instantiation, only a stale comment;
  the real UART lives under `hw/periph/`). So this address question may really be about a
  different subsystem's UART, not a SEP-internal mismatch — worth re-examining with that in mind.
- ~~PCR Vault (`PCRV`, `0x1091_4000` per the doc) is not modeled in the VP at all.~~ **Resolved
  (later session):** traced the full real RTL instantiation tree (`sep.sv` → `sep_crypto.sv` →
  leaf modules) — `PCRV`/`pcr_vault` appears nowhere under `hw/sep`. Confirmed absent from real
  RTL, not a VP gap.
- ~~A separate `TRNG` block (`0x1091_7000` per the doc) distinct from the modeled entropy source
  (`0x1091_6000`) may also be missing — unconfirmed.~~ **Resolved (later session):** `sep_crypto.sv`
  only has `ext_trng_axil_req_o`/`ext_trng_axis_req_i` — external passthrough *ports* muxed
  against `entropy_source`'s own output (see `entropy_src`'s VP model), not a separate on-die TRNG
  module. Confirmed there's nothing distinct to model here.

The UART/SPI address-swap lead was found by cross-referencing `ocah-documentation.md` against
`Args.hpp`, not by re-reading the real `hw/sep/*.sv` RTL tree, so treat it as a lead to verify,
not a confirmed bug. The PCRV/TRNG leads above were since verified directly against
`hw/sep/sep_crypto.sv` and are resolved.

**`sep/peripherals` inventory audit (later session):** cross-referenced every peripheral under
`sep/peripherals/` against real RTL instantiation sites (`sep.sv`, `sep_crypto.sv`,
`sep_system_peripherals.sv`, `sep_wdt_wrap.sv`, `sep_io.sv`). Every peripheral traces to a real
SEP submodule except `uart_16550` (confirmed not part of SEP — see above) and `spi_flash`, which
is a deliberate external-device behavioral stub (its own README: "SystemC model of an SPI NOR
flash device... has no direct memory-mapped bus address" — real silicon doesn't put the flash die
on SEP either, so a BFM is needed to exercise boot-from-SPI-flash tests; this one is correctly
placed, not miscategorized). Neither real crossbar (`sep_local_axi_xbar_wrapper`,
`sep_system_peripherals_xbar_wrapper`) is modeled as its own peripheral — both are collapsed into
the VP's single flat `SimpleBus`, consistent with the already-documented §2.1 simplification.

### 3.7 `sep_cpu_ctrl`'s timeout infrastructure has no counting/expiry logic
`TIMEOUT_COUNT_TROOT/DMA/SPACC/SYS_IN/MAILBOX_INBOUND/MAILBOX_OUTBOUND/ENTROPY_WRITE/ENTROPY_READ/
FILTER_OUT/ALIAS_REMAP` (8 distinct 48-bit counters) and `TIMEOUT_ENABLE` are plain writable
`regmodel::Memory` storage — nothing in the VP increments them, compares them against a threshold, or ever sets the
corresponding `hwif_in.*_timeout_int` flag as a consequence of elapsed time or a stalled
transaction. Only `TIMEOUT_CLEAR`'s clear-side callback is real (and correct — see the write-mask
fix noted in `sep_cpu_ctrl`'s design doc), but it has nothing to clear in practice.

**Impact:** a firmware test that deliberately stalls a monitored path (DMA, mailbox, alias-remap,
etc.) expecting `TIMEOUT_INTERRUPT` to eventually assert will not see that happen in this VP. The
only way to exercise the clear-side logic today is a test directly poking
`cpu_ctrl->hwif_in.<source>_timeout_int = true` from test code.

**Why deferred:** no current firmware test depends on this; adding real counting requires a
clock-sensitive `SC_METHOD`/`sc_spawn` per timeout source, tied to whatever "stall" condition each
source is supposed to detect (DMA stall, mailbox stall, etc.) — a nontrivial new piece of
behavior, not a small fix. Full detail in
`sep_cpu_ctrl/docs/02_sep_cpu_ctrl_HighLevel_Design.md` §2.2.1/§7.3/§8.2.

### 3.8 No CPU-only (WDT-gated) reset domain — `sep_reset_ctrl` is the real owner of this signal

Real RTL (`sep_reset_ctrl.sv`) computes `sep_cpu_reset_no = sep_reset_n & wdt_rst_ni` — a WDT bite
resets the CPU core and the warm reset domain derived from it, leaving ICCM/DCCM and the
peripherals driven by `sep_reset_n` alone untouched. `sep_scratch_warm` is *not* among them:
`sep_system_csr.sv` wires `u_sep_scratch_reg_warm` to `.arst_n(rst_ni && rst_warm_ni)` with
`rst_warm_ni = sep_cpu_reset_no`, so a WDT bite clears it, and
`sep_warm_cold_reset_scratch_test.py`'s `CHK-WARM-CLEAR` asserts that it does. The cold bank
takes `.arst_n(rst_ni)` and is the one that survives. This VP has a single flat
`reset_generation_unit` producing one `reset_signal` fanned out identically to every peripheral,
including the CPU — there is no second, CPU-only reset domain anywhere, and `sep_reset_ctrl_ip`
(the VP model of the peripheral that actually generates this signal in real hardware) has no
`wdt_rst_ni` input or `sep_cpu_reset_no`-equivalent output at all.

**Impact:** dormant today — confirmed no current firmware test depends on WDT-bite reset-domain
separation (see the `sep_scratch_warm`/`sep_scratch_cold` and `sep_cpu_ctrl` REFERENCE_COUNTER
investigations earlier this session). The moment a test asserts that SRAM/DCCM content survives
a WDT-triggered reset, this VP will diverge from real hardware. `scratch_warm` is not such a
test: it is cleared by a WDT bite on both sides, so the VP already agrees there. The cold bank
also agrees, since `sep_scratch_cold` takes its own power-on-only `cold_rst_ni` rather than the
global reset.

**Why deferred:** fixing it means adding a second reset domain VP-wide (a `cpu_reset_signal`
distinct from `reset_signal`, with the CPU/PIC on the narrow one and everything else on the
broad one), not just a `sep_reset_ctrl` change — nontrivial, and nothing currently exercises it.

---

## 4. Corrections made this session (previously-wrong VP-only scaffolding, now removed)

Not abstractions — genuine mistakes in earlier VP state, corrected and worth recording so they
aren't reintroduced:

- **AVBbus, GPIO, external CLINT, external PLIC** — fully removed from `sep/peripherals/` and
  `vp/platform/`. Real SEP silicon never had a PLIC or CLINT (confirmed via `och_sep_top_reg.h`
  having zero register entries for either); GPIO is a remote SMC-side peripheral, not SEP's own
  (confirmed independently via the OCAH doc's §5.2: "Remote peripherals shared with SMC: UART,
  GPIO"). These were VP-only scaffolding, never part of the real register map.
- **`irq_map.h`** rewritten to be an exact, RTL-verified mirror of the real 38-entry
  `sep_internal_interrupts[]` array; four VP-only synthetic IRQ constants (`UART_IRQ`,
  `AON_WKUP_IRQ`, `AON_WDOG_IRQ`, `SPI_ERROR_IRQ`) removed entirely rather than relocated.
- **`sep_filter_ctrl`'s `BlockByDefault`** was inverted (treated "no active entries" as
  passthrough instead of deny) — this was the most severe bug found this session; fixed to match
  `axi_filter_wrap.sv`'s real `BlockByDefault=1'b1` deny-always-on-no-match semantics.
